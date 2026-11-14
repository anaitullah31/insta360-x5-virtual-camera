#include <QApplication>
#include <QImage>
#include <QPixmap>
#include <QElapsedTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QComboBox>
#include <QSizePolicy>
#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QWidget>
#include <QTimer>
#include <QDebug>
#include <QPointer>
#include <ins_realtime_stitcher.h>
#include <ins_stitcher.h>
#include <stream/stream_delegate.h>
#include <camera/camera.h>
#include <camera/device_discovery.h>
#include <atomic>
#include <memory>
#include <vector>
#include <functional>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <exception>

class VirtualViewLabel final : public QLabel {
public:
    std::function<void()> viewChanged;
    double yaw = 0.0;
    double pitch = 0.0;
    double fov = 90.0;
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
        QLabel::mousePressEvent(e);
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (dragging_) {
            const QPoint delta = e->pos() - lastPos_;
            lastPos_ = e->pos();
            yaw -= delta.x() * 0.005;
            pitch = std::clamp(pitch + delta.y() * 0.005, -1.45, 1.45);
            if (viewChanged) viewChanged();
        }
        QLabel::mouseMoveEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            dragging_ = false;
            setCursor(Qt::OpenHandCursor);
        }
        QLabel::mouseReleaseEvent(e);
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

static QImage perspectiveView(const QImage& panorama, double yaw,
                              double pitch, double horizontalFov) {
    if (panorama.isNull()) return {};
    const QImage src = panorama.format() == QImage::Format_RGBA8888
        ? panorama : panorama.convertToFormat(QImage::Format_RGBA8888);
    constexpr int outW = 1280, outH = 720;
    QImage out(outW, outH, QImage::Format_RGBA8888);
    if (out.isNull()) return {};
    const double pi = 3.14159265358979323846;
    const double scale = std::tan(horizontalFov * pi / 360.0);
    const double cp = std::cos(pitch), sp = std::sin(pitch);
    const double cy = std::cos(yaw), sy = std::sin(yaw);
    const int sw = src.width(), sh = src.height();
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
            const double fx = (lon / (2.0*pi) + 0.5) * sw - 0.5;
            const double fy = std::clamp((0.5 - lat/pi) * sh - 0.5, 0.0, double(sh - 1));
            const int x0raw = static_cast<int>(std::floor(fx));
            const int x0 = ((x0raw % sw) + sw) % sw;
            const int x1 = (x0 + 1) % sw;
            const int y0 = static_cast<int>(fy);
            const int y1 = std::min(y0 + 1, sh - 1);
            const double tx = fx - std::floor(fx), ty = fy - y0;
            const auto* a = src.constScanLine(y0) + x0 * 4;
            const auto* b = src.constScanLine(y0) + x1 * 4;
            const auto* c = src.constScanLine(y1) + x0 * 4;
            const auto* d = src.constScanLine(y1) + x1 * 4;
            for (int channel = 0; channel < 4; ++channel) {
                const double top = a[channel] * (1.0 - tx) + b[channel] * tx;
                const double bottom = c[channel] * (1.0 - tx) + d[channel] * tx;
                dst[x * 4 + channel] = static_cast<unsigned char>(
                    std::clamp(top * (1.0 - ty) + bottom * ty, 0.0, 255.0));
            }
        }
    }
    return out;
}

// Pixel format values below are provisional mappings; the Media SDK callback
// may use its own identifiers. The packed fmt=0 case is based on observed logs.
static QImage convertStitchedFrame(uint8_t* planes[4], int stride[4],
                                   int w, int h, int fmt) {
    if (!planes[0] || w <= 0 || h <= 0 || w > 8192 || h > 8192) return {};

    // Observed Insta360 Media SDK callback on this X5:
    // fmt=0, 1920x960, stride[0]=7680, only planes[0] populated.
    // This is a packed 4-byte-per-pixel buffer, not planar YUV420P.
    // Channel order is provisionally RGBA; verify colors on the camera preview.
    if (fmt == 0 && !planes[1] && !planes[2] &&
        stride[0] >= w * 4) {
        QImage wrapped(planes[0], w, h, stride[0], QImage::Format_RGBA8888);
        return wrapped.copy(); // Own the data before SDK reuses its buffer.
    }

    if (fmt == 26 || fmt == 28 || fmt == 2 || fmt == 3) {
        const int bytes = (fmt == 26 || fmt == 28) ? 4 : 3;
        if (stride[0] < w * bytes) return {};
        const auto qfmt = fmt == 26 ? QImage::Format_RGBA8888 :
                          fmt == 28 ? QImage::Format_RGBA8888 :
                          fmt == 2 ? QImage::Format_RGB888 : QImage::Format_BGR888;
        QImage image(planes[0], w, h, stride[0], qfmt);
        QImage result = image.copy();
        if (fmt == 28) {
            // BGRA bytes -> RGBA bytes (swap red and blue in memory).
            for (int y = 0; y < result.height(); ++y) {
                auto* row = result.scanLine(y);
                for (int x = 0; x < result.width(); ++x)
                    std::swap(row[x * 4], row[x * 4 + 2]);
            }
        }
        return result;
    }
    if (fmt != 0 && fmt != 23) return {};
    if (!planes[1] || (fmt == 0 && !planes[2])) return {};
    if (stride[0] < w || stride[1] < ((w + 1) / 2) * (fmt == 23 ? 2 : 1) ||
        (fmt == 0 && stride[2] < (w + 1) / 2)) return {};
    QImage result(w, h, QImage::Format_RGBA8888);
    if (result.isNull()) return {};
    auto clampByte = [](int n) {
        return static_cast<unsigned char>(std::clamp(n, 0, 255));
    };
    for (int y = 0; y < h; ++y) {
        auto* dst = result.scanLine(y);
        const auto* yy = planes[0] + y * stride[0];
        const auto* uv = planes[1] + (y / 2) * stride[1];
        const auto* vv = fmt == 0 ? planes[2] + (y / 2) * stride[2] : nullptr;
        for (int x = 0; x < w; ++x) {
            const int Y = std::max(0, int(yy[x]) - 16);
            const int U = int(uv[(x / 2) * (fmt == 23 ? 2 : 1)]) - 128;
            const int V = int(fmt == 23 ? uv[(x / 2) * 2 + 1] : vv[x / 2]) - 128;
            const int C = 298 * Y;
            dst[x * 4]     = clampByte((C + 409 * V + 128) >> 8);
            dst[x * 4 + 1] = clampByte((C - 100 * U - 208 * V + 128) >> 8);
            dst[x * 4 + 2] = clampByte((C + 516 * U + 128) >> 8);
            dst[x * 4 + 3] = 255;
        }
    }
    return result;
}

struct StitchedFrameBuffer {
    QMutex mutex;
    QImage latest;
    std::atomic<unsigned long long> frames{0};
    std::atomic<int> lastFormat{-1};
    std::atomic<unsigned long long> unsupported{0};
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
        if (!data.empty())
            std::memcpy(converted.data(), data.data(),
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
    QLabel* status_ = nullptr;
    QLabel* previewStatus_ = nullptr;
    QPushButton *previewStart_ = nullptr, *previewStop_ = nullptr;
    std::shared_ptr<StitchReceiver> receiver_;
    std::shared_ptr<ins::RealTimeStitcher> stitcher_;
    std::shared_ptr<StitchedFrameBuffer> frameBuffer_;
    VirtualViewLabel* videoLabel_ = nullptr;
    QComboBox* viewMode_ = nullptr;
    QComboBox* qualityMode_ = nullptr;
    QPushButton* resetView_ = nullptr;
    QPushButton *connect_ = nullptr, *photo_ = nullptr, *start_ = nullptr,
                *stop_ = nullptr, *disconnect_ = nullptr;
    unsigned long long lastDisplayedFrame_ = 0;
    unsigned long long lastMeasuredFrame_ = 0;
    QElapsedTimer fpsClock_;
    double measuredFps_ = 0.0;
    QImage lastPanorama_;
    bool previewing_ = false;

    void showFrame() {
        if (lastPanorama_.isNull()) return;
        QImage image = viewMode_->currentIndex() == 1
            ? perspectiveView(lastPanorama_, videoLabel_->yaw,
                              videoLabel_->pitch, videoLabel_->fov)
            : lastPanorama_;
        if (!image.isNull())
            videoLabel_->setPixmap(QPixmap::fromImage(image).scaled(
                videoLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    void updateButtons() {
        const bool ready = static_cast<bool>(camera_);
        connect_->setEnabled(!ready);
        disconnect_->setEnabled(ready);
        photo_->setEnabled(ready);
        start_->setEnabled(ready);
        stop_->setEnabled(ready);
        previewStart_->setEnabled(ready && !previewing_);
        previewStop_->setEnabled(ready && previewing_);
        qualityMode_->setEnabled(!previewing_);
    }
    void error(const QString& message) {
        QMessageBox::warning(this, "X5 Control Center", message);
        status_->setText(message);
    }
    void stopPreview() {
        if (!previewing_ || !camera_) return;
        camera_->StopLiveStreaming();
        if (stitcher_) stitcher_->CancelStitch();
        previewing_ = false;
        receiver_.reset();
        stitcher_.reset();
        frameBuffer_.reset();
        lastPanorama_ = QImage();
        videoLabel_->setPixmap(QPixmap());
        videoLabel_->setText("Preview stopped.");
        lastDisplayedFrame_ = lastMeasuredFrame_ = 0;
        measuredFps_ = 0.0;
        previewStatus_->setText("Live 360 panorama stopped.");
        updateButtons();
    }
public:
    X5Window() {
        setWindowTitle("Insta360 X5 Control Center V2 - Camera SDK 2.1.8");
        resize(1000, 720);
        auto* body = new QWidget;
        auto* layout = new QVBoxLayout(body);
        auto* title = new QLabel("Insta360 X5 | USB Camera Controller");
        QFont font = title->font();
        font.setPointSize(17); font.setBold(true);
        title->setFont(font);
        layout->addWidget(title);
        status_ = new QLabel("Disconnected. Connect X5 via USB-C in SDK-compatible USB mode.");
        status_->setWordWrap(true);
        layout->addWidget(status_);
        auto* row = new QHBoxLayout;
        connect_ = new QPushButton("Connect");
        disconnect_ = new QPushButton("Disconnect");
        photo_ = new QPushButton("Take Photo");
        start_ = new QPushButton("Start Recording");
        stop_ = new QPushButton("Stop Recording");
        for (auto* button : {connect_, disconnect_, photo_, start_, stop_})
            row->addWidget(button);
        layout->addLayout(row);
        auto* previewRow = new QHBoxLayout;
        previewStart_ = new QPushButton("Start Preview");
        previewStop_ = new QPushButton("Stop Preview");
        previewRow->addWidget(previewStart_);
        previewRow->addWidget(previewStop_);
        layout->addLayout(previewRow);
        previewStatus_ = new QLabel("Preview not started.");
        previewStatus_->setWordWrap(true);
        layout->addWidget(previewStatus_);
        auto* modeRow = new QHBoxLayout;
        modeRow->addWidget(new QLabel("Viewing mode:"));
        viewMode_ = new QComboBox;
        viewMode_->addItem("Full 360 panorama");
        viewMode_->addItem("Virtual 16:9 camera (drag / scroll)");
        modeRow->addWidget(viewMode_, 1);
        layout->addLayout(modeRow);
        auto* qualityRow = new QHBoxLayout;
        qualityRow->addWidget(new QLabel("Preview quality:"));
        qualityMode_ = new QComboBox;
        qualityMode_->addItem("Balanced 1920x960 @30 (recommended)");
        qualityMode_->addItem("High 2560x1280 @30");
        qualityMode_->addItem("4K panorama 3840x1920 @30 (experimental)");
        qualityMode_->addItem("Compatibility 1440x720 @30");
        qualityRow->addWidget(qualityMode_, 1);
        resetView_ = new QPushButton("Reset view");
        qualityRow->addWidget(resetView_);
        layout->addLayout(qualityRow);
        videoLabel_ = new VirtualViewLabel;
        videoLabel_->setText("Connect your X5, then click Start Preview.");
        videoLabel_->viewChanged = [this] {
            if (viewMode_->currentIndex() == 1) showFrame();
        };
        connect(resetView_, &QPushButton::clicked, this, [this] {
            videoLabel_->yaw = 0;
            videoLabel_->pitch = 0;
            videoLabel_->fov = 90;
            showFrame();
        });
        connect(viewMode_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { showFrame(); });
        layout->addWidget(videoLabel_, 1);
        auto* note = new QLabel("Select Virtual 16:9 camera, drag to look around, and scroll to zoom.");
        note->setWordWrap(true);
        layout->addWidget(note);
        setCentralWidget(body);

        connect(connect_, &QPushButton::clicked, this, [this] {
            try {
                ins_camera::DeviceDiscovery discovery;
                auto devices = discovery.GetAvailableDevices();
                if (devices.empty()) {
                    error("No camera detected. Check USB mode and cable.");
                    return;
                }
                auto candidate = std::make_shared<ins_camera::Camera>(devices[0].info);
                const bool opened = candidate->Open();
                discovery.FreeDeviceDescriptors(devices);
                if (!opened) {
                    error("Camera detected but Open() failed.");
                    return;
                }
                camera_ = std::move(candidate);
                status_->setText("Connected to camera. SDK version: " +
                                 QString::fromStdString(ins_camera::GetSDKVersion()));
                updateButtons();
            } catch (const std::exception& e) {
                error(QString("Connection error: ") + e.what());
            }
        });
        connect(disconnect_, &QPushButton::clicked, this, [this] {
            if (camera_) {
                stopPreview();
                camera_->Close();
                camera_.reset();
            }
            status_->setText("Disconnected");
            updateButtons();
        });
        connect(photo_, &QPushButton::clicked, this, [this] {
            if (!camera_) return;
            try {
                if (camera_->CaptureCurrentStatus()) {
                    error("Camera is already capturing.");
                    return;
                }
                camera_->TakePhoto();
                status_->setText("Photo command completed. Check camera storage / SDK log.");
            } catch (const std::exception& e) { error(e.what()); }
        });
        connect(start_, &QPushButton::clicked, this, [this] {
            if (!camera_) return;
            try {
                if (camera_->CaptureCurrentStatus()) {
                    error("Camera is already capturing.");
                    return;
                }
                const bool ok = camera_->StartRecording();
                status_->setText(ok ? "Recording started" :
                                 "Start recording failed (check video mode/settings)");
            } catch (const std::exception& e) { error(e.what()); }
        });
        connect(stop_, &QPushButton::clicked, this, [this] {
            if (!camera_) return;
            try {
                if (!camera_->CaptureCurrentStatus()) {
                    error("Camera is not recording.");
                    return;
                }
                camera_->StopRecording();
                status_->setText("Stop recording command completed. Check camera storage.");
            } catch (const std::exception& e) { error(e.what()); }
        });
        connect(previewStart_, &QPushButton::clicked, this, [this] {
            if (!camera_ || previewing_) return;
            try {
                const auto preview = camera_->GetPreviewParam();
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
                const int profile = qualityMode_->currentIndex();
                const int outputW = profile == 2 ? 3840 : profile == 1 ? 2560 :
                                    profile == 3 ? 1440 : 1920;
                stitcher_->SetOutputSize(outputW, outputW / 2);
                frameBuffer_ = std::make_shared<StitchedFrameBuffer>();
                auto buffer = frameBuffer_;
                stitcher_->SetStitchRealTimeDataCallback(
                    [buffer](uint8_t* data[4], int linesize[4], int width,
                             int height, int format, int64_t) {
                        buffer->lastFormat.store(format);
                        QImage image = convertStitchedFrame(data, linesize,
                                                             width, height, format);
                        if (image.isNull()) {
                            const auto rejected = ++buffer->unsupported;
                            if (rejected <= 5 || rejected % 300 == 0) {
                                qWarning() << "STITCH FRAME REJECTED"
                                           << "format:" << format
                                           << "width:" << width
                                           << "height:" << height
                                           << "stride0:" << linesize[0]
                                           << "stride1:" << linesize[1]
                                           << "stride2:" << linesize[2]
                                           << "plane0:" << (data[0] != nullptr)
                                           << "plane1:" << (data[1] != nullptr)
                                           << "plane2:" << (data[2] != nullptr);
                            }
                            return;
                        }
                        {
                            QMutexLocker lock(&buffer->mutex);
                            buffer->latest = std::move(image);
                            ++buffer->frames;
                        }
                    });
                receiver_ = std::make_shared<StitchReceiver>(stitcher_);
                std::shared_ptr<ins_camera::StreamDelegate> delegate = receiver_;
                camera_->SetStreamDelegate(delegate);
                ins_camera::LiveStreamParam param;
                param.video_resolution = profile == 2
                    ? ins_camera::VideoResolution::RES_3840_1920P30
                    : profile == 1 ? ins_camera::VideoResolution::RES_2560_1280P30
                    : profile == 3 ? ins_camera::VideoResolution::RES_1440_720P30
                    : ins_camera::VideoResolution::RES_1920_960P30;
                param.lrv_video_resulution = ins_camera::VideoResolution::RES_1440_720P30;
                param.video_bitrate = profile == 2 ? 20 * 1024 * 1024
                                    : profile == 1 ? 12 * 1024 * 1024
                                    : profile == 3 ? 2 * 1024 * 1024
                                    : 6 * 1024 * 1024;
                param.enable_audio = false;
                param.using_lrv = false;
                if (!camera_->StartLiveStreaming(param)) {
                    receiver_.reset();
                    stitcher_.reset();
                    frameBuffer_.reset();
                    error("StartLiveStreaming failed for this quality profile. Try Balanced or Compatibility.");
                    return;
                }
                stitcher_->StartStitch();
                previewing_ = true;
                lastDisplayedFrame_ = lastMeasuredFrame_ = 0;
                fpsClock_.restart();
                previewStatus_->setText("Live 360 stitching started; waiting for frames...");
                updateButtons();
            } catch (const std::exception& e) {
                if (camera_) camera_->StopLiveStreaming();
                if (stitcher_) stitcher_->CancelStitch();
                receiver_.reset();
                stitcher_.reset();
                frameBuffer_.reset();
                error(QString("Stitching error: ") + e.what());
            }
        });
        connect(previewStop_, &QPushButton::clicked, this, [this] { stopPreview(); });
        auto* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this] {
            if (!previewing_ || !frameBuffer_) return;
            const auto frameCount = frameBuffer_->frames.load();
            if (frameCount != lastDisplayedFrame_) {
                QImage image;
                {
                    QMutexLocker lock(&frameBuffer_->mutex);
                    image = frameBuffer_->latest;
                }
                if (!image.isNull()) {
                    lastPanorama_ = std::move(image);
                    lastDisplayedFrame_ = frameCount;
                    showFrame();
                }
            }
            if (fpsClock_.elapsed() >= 1000) {
                measuredFps_ = 1000.0 * (frameCount - lastMeasuredFrame_)
                    / std::max<qint64>(1, fpsClock_.elapsed());
                lastMeasuredFrame_ = frameCount;
                fpsClock_.restart();
                previewStatus_->setText(
                    QString("Stitched: %1x%2 | %3 fps | %4 frames | pixel format %5 | rejected %6")
                    .arg(lastPanorama_.width()).arg(lastPanorama_.height())
                    .arg(measuredFps_, 0, 'f', 1).arg(frameCount)
                    .arg(frameBuffer_->lastFormat.load())
                    .arg(frameBuffer_->unsupported.load()));
            }
        });
        timer->start(33);
        updateButtons();
    }
    ~X5Window() override {
        if (camera_) {
            stopPreview();
            camera_->Close();
        }
    }
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    ins::InitEnv();
    const QString modelPath = QCoreApplication::applicationDirPath() + "/models/";
    ins::SetModelFileRootDir(modelPath.toStdString());
    X5Window window;
    window.show();
    return app.exec();
}
