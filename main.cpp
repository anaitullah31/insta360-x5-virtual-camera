#include <QApplication>
#include <QImage>
#include <QPixmap>
#include <QMutex>
#include <QMutexLocker>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QComboBox>
#include <QSizePolicy>
#include <functional>
#include <cmath>
#include <algorithm>
#include <ins_realtime_stitcher.h>
#include <ins_stitcher.h>
#include <cstring>
#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QWidget>
#include <QTimer>
#include <QDir>
#include <QStandardPaths>
#include <stream/stream_delegate.h>
#include <camera/photography_settings.h>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <map>
#include <camera/camera.h>
#include <stream/stream_delegate.h>
#include <camera/device_discovery.h>
#include <memory>
#include <vector>
#include <exception>


// Mouse-controlled virtual camera. The SDK still provides a full 2:1 panorama;
// this widget selects a perspective view without changing the camera stream.
class VirtualViewLabel final : public QLabel {
public:
    std::function<void()> viewChanged;
    double yaw = 0.0;   // radians
    double pitch = 0.0; // radians
    double fov = 90.0;  // horizontal field of view, degrees

    explicit VirtualViewLabel(QWidget* parent = nullptr) : QLabel(parent) {
        setAlignment(Qt::AlignCenter);
        setMinimumSize(640, 360);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setStyleSheet("background:#171717; color:white;");
        setCursor(Qt::OpenHandCursor);
    }
protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            lastPos_ = e->pos(); dragging_ = true;
            setCursor(Qt::ClosedHandCursor);
        }
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (!dragging_) return;
        const QPoint delta = e->pos() - lastPos_;
        lastPos_ = e->pos();
        yaw -= delta.x() * 0.005;
        pitch = std::clamp(pitch + delta.y() * 0.005, -1.45, 1.45);
        if (viewChanged) viewChanged();
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            dragging_ = false; setCursor(Qt::OpenHandCursor);
        }
    }
    void wheelEvent(QWheelEvent* e) override {
        fov = std::clamp(fov - (e->angleDelta().y() / 120.0) * 5.0, 35.0, 120.0);
        if (viewChanged) viewChanged();
        e->accept();
    }
private:
    bool dragging_ = false;
    QPoint lastPos_;
};

// Reproject a 2:1 equirectangular panorama into a normal 16:9 perspective.
// Nearest-neighbour sampling is intentional for a first, CPU-only prototype.
static QImage perspectiveView(const QImage& panorama, double yaw,
                              double pitch, double horizontalFov) {
    if (panorama.isNull()) return {};
    const QImage src = panorama.format() == QImage::Format_RGBA8888
        ? panorama : panorama.convertToFormat(QImage::Format_RGBA8888);
    constexpr int outW = 640, outH = 360;
    QImage out(outW, outH, QImage::Format_RGBA8888);
    const double pi = 3.14159265358979323846;
    const double scale = std::tan(horizontalFov * pi / 360.0);
    const double cp = std::cos(pitch), sp = std::sin(pitch);
    const double cy = std::cos(yaw), sy = std::sin(yaw);
    for (int y = 0; y < outH; ++y) {
        auto* dst = out.scanLine(y);
        const double v = (1.0 - 2.0 * (y + 0.5) / outH) * scale * outH / outW;
        for (int x = 0; x < outW; ++x) {
            const double u = (2.0 * (x + 0.5) / outW - 1.0) * scale;
            const double norm = std::sqrt(u*u + v*v + 1.0);
            const double dx = u / norm, dy = v / norm, dz = 1.0 / norm;
            const double ry = dy * cp + dz * sp;
            const double rz = dz * cp - dy * sp;
            const double rx = dx * cy + rz * sy;
            const double rz2 = rz * cy - dx * sy;
            const double lon = std::atan2(rx, rz2);
            const double lat = std::asin(std::clamp(ry, -1.0, 1.0));
            int sx = static_cast<int>((lon / (2.0*pi) + 0.5) * src.width());
            sx = ((sx % src.width()) + src.width()) % src.width();
            int syi = std::clamp(static_cast<int>((0.5 - lat/pi) * src.height()),
                                 0, src.height() - 1);
            const auto* pixel = src.constScanLine(syi) + sx * 4;
            std::memcpy(dst + x * 4, pixel, 4);
        }
    }
    return out;
}

// Shared image buffer: SDK callbacks never touch Qt widgets directly.
struct StitchedFrameBuffer {
    QMutex mutex;
    QImage latest;
    std::atomic<unsigned long long> frames{0};
};

class StitchReceiver final : public ins_camera::StreamDelegate {
public:
    explicit StitchReceiver(std::shared_ptr<ins::RealTimeStitcher> stitcher)
        : stitcher_(std::move(stitcher)) {}
    void OnVideoData(const uint8_t* data, size_t size, int64_t timestamp,
                     uint8_t type, int index) override {
        stitcher_->HandleVideoData(data, size, timestamp, type, index);
    }
    void OnAudioData(const uint8_t*, size_t, int64_t) override {}
    void OnGyroData(const std::vector<ins_camera::GyroData>& data) override {
        std::vector<ins::GyroData> converted(data.size());
        static_assert(sizeof(ins::GyroData) == sizeof(ins_camera::GyroData),
                      "Gyro structures must match");
        if (!data.empty()) std::memcpy(converted.data(), data.data(),
                                       data.size() * sizeof(ins_camera::GyroData));
        stitcher_->HandleGyroData(converted);
    }
    void OnExposureData(const ins_camera::ExposureData& data) override {
        ins::ExposureData exposure{};
        exposure.exposure_time = data.exposure_time;
        exposure.timestamp = data.timestamp;
        stitcher_->HandleExposureData(exposure);
    }
private:
    std::shared_ptr<ins::RealTimeStitcher> stitcher_;
};

class X5Window : public QMainWindow {
    std::shared_ptr<ins_camera::Camera> camera_;
    QLabel *status_;
    QLabel *previewStatus_;
    QPushButton *previewStart_, *previewStop_;
    std::shared_ptr<StitchReceiver> receiver_;
    std::shared_ptr<ins::RealTimeStitcher> stitcher_;
    std::shared_ptr<StitchedFrameBuffer> frameBuffer_;
    VirtualViewLabel* videoLabel_ = nullptr;
    QComboBox* viewMode_ = nullptr;
    QImage lastPanorama_;
    void showFrame() {
        if (lastPanorama_.isNull()) return;
        const bool virtualMode = viewMode_->currentIndex() == 1;
        QImage image = virtualMode
            ? perspectiveView(lastPanorama_, videoLabel_->yaw, videoLabel_->pitch, videoLabel_->fov)
            : lastPanorama_;
        videoLabel_->setPixmap(QPixmap::fromImage(image).scaled(
            videoLabel_->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
    }
    bool previewing_ = false;


    void stopPreview() {
        if (!previewing_ || !camera_) return;
        camera_->StopLiveStreaming();
        if (stitcher_) stitcher_->CancelStitch();
        previewing_ = false;
        receiver_.reset();
        stitcher_.reset();
        frameBuffer_.reset();
        lastPanorama_ = QImage();
        previewStatus_->setText("Live 360 panorama stopped.");
        updateButtons();
    }
    QPushButton *connect_, *photo_, *start_, *stop_, *disconnect_;
    void updateButtons() {
        bool ready = static_cast<bool>(camera_);
        connect_->setEnabled(!ready);
        disconnect_->setEnabled(ready);
        photo_->setEnabled(ready);
        start_->setEnabled(ready);
        stop_->setEnabled(ready);
        previewStart_->setEnabled(ready && !previewing_);
        previewStop_->setEnabled(ready && previewing_);
    }
    void error(const QString& message) { QMessageBox::warning(this, "X5 Control Center", message); status_->setText(message); }
public:
    X5Window() {
        setWindowTitle("Insta360 X5 Control Center - Camera SDK 2.1.8");
        resize(1000, 720);
        auto *body = new QWidget;
        auto *layout = new QVBoxLayout(body);
        auto *title = new QLabel("Insta360 X5 | USB Camera Controller");
        QFont font = title->font(); font.setPointSize(17); font.setBold(true); title->setFont(font);
        layout->addWidget(title);
        status_ = new QLabel("Disconnected. Connect X5 via USB-C in SDK-compatible USB mode.");
        status_->setWordWrap(true); layout->addWidget(status_);
        auto *row = new QHBoxLayout;
        connect_ = new QPushButton("Connect"); disconnect_ = new QPushButton("Disconnect");
        photo_ = new QPushButton("Take Photo"); start_ = new QPushButton("Start Recording"); stop_ = new QPushButton("Stop Recording");
        for (auto *button : {connect_, disconnect_, photo_, start_, stop_}) row->addWidget(button);
        layout->addLayout(row);
        auto *previewRow = new QHBoxLayout;
        previewStart_ = new QPushButton("Start Preview");
        previewStop_ = new QPushButton("Stop Preview");
        previewRow->addWidget(previewStart_);
        previewRow->addWidget(previewStop_);
        layout->addLayout(previewRow);
        previewStatus_ = new QLabel("Preview not started.");
        previewStatus_->setWordWrap(true);
        layout->addWidget(previewStatus_);
        auto *modeRow = new QHBoxLayout;
        modeRow->addWidget(new QLabel("Viewing mode:"));
        viewMode_ = new QComboBox;
        viewMode_->addItem("Full 360 panorama");
        viewMode_->addItem("Virtual 16:9 camera (drag / scroll)");
        modeRow->addWidget(viewMode_, 1);
        layout->addLayout(modeRow);
        videoLabel_ = new VirtualViewLabel;
        videoLabel_->setText("Connect your X5, then click Start Preview.");
        videoLabel_->viewChanged = [this] { if (viewMode_->currentIndex() == 1) showFrame(); };
        connect(viewMode_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { showFrame(); });
        layout->addWidget(videoLabel_, 1);
        auto *note = new QLabel("Select Virtual 16:9 camera, drag to look around, and scroll to zoom.");
        note->setWordWrap(true);
        layout->addWidget(note);
        setCentralWidget(body);
        connect(connect_, &QPushButton::clicked, this, [this] {
            try {
                ins_camera::DeviceDiscovery discovery;
                auto devices = discovery.GetAvailableDevices();
                if (devices.empty()) { error("No camera detected. Check USB mode and cable."); return; }
                auto candidate = std::make_shared<ins_camera::Camera>(devices[0].info);
                bool opened = candidate->Open();
                discovery.FreeDeviceDescriptors(devices);
                if (!opened) { error("Camera detected but Open() failed."); return; }
                camera_ = std::move(candidate);
                status_->setText("Connected to camera. SDK version: " + QString::fromStdString(ins_camera::GetSDKVersion()));
                updateButtons();
            } catch (const std::exception& e) { error(QString("Connection error: ") + e.what()); }
        });
        connect(disconnect_, &QPushButton::clicked, this, [this] {
            if (camera_) { stopPreview(); camera_->Close(); camera_.reset(); }
            status_->setText("Disconnected"); updateButtons();
        });
        connect(photo_, &QPushButton::clicked, this, [this] {
            if (!camera_) return;
            try {
                if (camera_->CaptureCurrentStatus()) { error("Camera is already capturing."); return; }
                auto result = camera_->TakePhoto();
                status_->setText("Photo command completed. Check camera storage / SDK log.");
            } catch (const std::exception& e) { error(e.what()); }
        });
        connect(start_, &QPushButton::clicked, this, [this] {
            if (!camera_) return;
            try {
                if (camera_->CaptureCurrentStatus()) { error("Camera is already capturing."); return; }
                bool ok = camera_->StartRecording();
                status_->setText(ok ? "Recording started" : "Start recording failed (check video mode/settings)");
            } catch (const std::exception& e) { error(e.what()); }
        });
        connect(stop_, &QPushButton::clicked, this, [this] {
            if (!camera_) return;
            try {
                if (!camera_->CaptureCurrentStatus()) { error("Camera is not recording."); return; }
                auto result = camera_->StopRecording();
                status_->setText("Stop recording command completed. Check camera storage.");
            } catch (const std::exception& e) { error(e.what()); }
        });
        connect(previewStart_, &QPushButton::clicked, this, [this] {
            if (!camera_ || previewing_) return;
            try {
                auto preview = camera_->GetPreviewParam();
                ins::CameraInfo info;
                info.cameraName = preview.camera_name;
                info.decode_type = static_cast<ins::VideoDecodeType>(preview.encode_type);
                std::vector<std::string> calibration;
                for (size_t i = 0; i < preview.GetCalibrationCount(); ++i)
                    calibration.push_back(preview.GetCalibration(i));
                info.SetCalibration(calibration,
                    preview.GetCropSrcWidth(), preview.GetCropSrcHeight(),
                    preview.GetCropDstWidth(), preview.GetCropDstHeight(),
                    preview.GetCropOffsetX(), preview.GetCropOffsetY());

                stitcher_ = std::make_shared<ins::RealTimeStitcher>();
                stitcher_->SetCameraInfo(info);
                stitcher_->SetStitchType(ins::STITCH_TYPE::DYNAMICSTITCH);
                stitcher_->EnableFlowState(true);
                stitcher_->SetOutputSize(960, 480);
                frameBuffer_ = std::make_shared<StitchedFrameBuffer>();
                auto buffer = frameBuffer_;
                stitcher_->SetStitchRealTimeDataCallback(
                    [buffer](uint8_t* data[4], int linesize[4], int width,
                             int height, int, int64_t) {
                        if (!data[0] || width <= 0 || height <= 0) return;
                        QImage image(data[0], width, height, linesize[0],
                                     QImage::Format_RGBA8888);
                        QMutexLocker lock(&buffer->mutex);
                        buffer->latest = image.copy(); // SDK owns original bytes
                        ++buffer->frames;
                    });
                receiver_ = std::make_shared<StitchReceiver>(stitcher_);
                std::shared_ptr<ins_camera::StreamDelegate> delegate = receiver_;
                camera_->SetStreamDelegate(delegate);

                ins_camera::LiveStreamParam param;
                param.video_resolution = ins_camera::VideoResolution::RES_1440_720P30;
                param.lrv_video_resulution = ins_camera::VideoResolution::RES_1440_720P30;
                param.video_bitrate = 1024 * 1024 / 2;
                param.enable_audio = false;
                param.using_lrv = false;
                if (!camera_->StartLiveStreaming(param)) {
                    receiver_.reset(); stitcher_.reset(); frameBuffer_.reset();
                    error("StartLiveStreaming failed.");
                    return;
                }
                stitcher_->StartStitch();
                previewing_ = true;
                previewStatus_->setText("Live 360 stitching started; waiting for frames...");
                updateButtons();
            } catch (const std::exception& e) {
                if (camera_) camera_->StopLiveStreaming();
                if (stitcher_) stitcher_->CancelStitch();
                receiver_.reset(); stitcher_.reset(); frameBuffer_.reset();
                error(QString("Stitching error: ") + e.what());
            }
        });
        connect(previewStop_, &QPushButton::clicked, this, [this] { stopPreview(); });
        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this] {
            if (!previewing_ || !frameBuffer_) return;
            QImage image;
            {
                QMutexLocker lock(&frameBuffer_->mutex);
                image = frameBuffer_->latest;
            }
            if (!image.isNull()) {
                lastPanorama_ = image;
                showFrame();
            }
            previewStatus_->setText(QString("Live stitched panorama: %1 frames received")
                                    .arg(frameBuffer_->frames.load()));
        });
        timer->start(100);
        updateButtons();
    }
    ~X5Window() override { if (camera_) { stopPreview(); camera_->Close(); } }
};
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    ins::InitEnv();
    const QString modelPath = QCoreApplication::applicationDirPath() + "/models/";
    ins::SetModelFileRootDir(modelPath.toStdString());
    X5Window window;
    window.show();
    return app.exec();
}
