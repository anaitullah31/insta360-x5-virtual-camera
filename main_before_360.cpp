#include <QApplication>
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


// Receives encoded video from the SDK. The SDK may call this from a worker thread.
class PreviewReceiver final : public ins_camera::StreamDelegate {
public:
    explicit PreviewReceiver(const QString& directory) : directory_(directory) {}
    std::atomic<unsigned long long> packets{0};
    std::atomic<unsigned long long> bytes{0};
    std::atomic<int> lastStream{-1};

    void OnVideoData(const uint8_t* data, size_t size, int64_t, uint8_t, int stream_index) override {
        if (!data || size == 0) return;
        std::lock_guard<std::mutex> lock(mutex_);
        auto& output = files_[stream_index];
        if (!output.is_open()) {
            QString name = directory_ + "/preview_stream_" + QString::number(stream_index) + ".h264";
            output.open(std::filesystem::path(name.toStdWString()), std::ios::binary | std::ios::trunc);
        }
        if (output.is_open()) {
            output.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
            ++packets;
            bytes += static_cast<unsigned long long>(size);
            lastStream = stream_index;
        }
    }
    void OnAudioData(const uint8_t*, size_t, int64_t) override {}
    void OnGyroData(const std::vector<ins_camera::GyroData>&) override {}
    void OnExposureData(const ins_camera::ExposureData&) override {}

private:
    QString directory_;
    std::mutex mutex_;
    std::map<int, std::ofstream> files_;
};

class X5Window : public QMainWindow {
    std::shared_ptr<ins_camera::Camera> camera_;
    QLabel *status_;
    QLabel *previewStatus_;
    QPushButton *previewStart_, *previewStop_;
    std::shared_ptr<PreviewReceiver> receiver_;
    bool previewing_ = false;
    QString previewDirectory_;

    void stopPreview() {
        if (!previewing_ || !camera_) return;
        camera_->StopLiveStreaming();
        previewing_ = false;
        receiver_.reset();
        previewStatus_->setText("Preview stopped. Saved H.264 files in: " + previewDirectory_);
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
        resize(640, 360);
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
        auto *note = new QLabel("Preview test: saves encoded H.264 stream to your PC (no on-screen video yet).\n"
                                 "Captured files are stored on the camera; download support is a future step.");
        note->setWordWrap(true); layout->addWidget(note); layout->addStretch();
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
            previewDirectory_ = QDir::homePath() + "/X5Preview";
            if (!QDir().mkpath(previewDirectory_)) {
                error("Cannot create preview output folder: " + previewDirectory_);
                return;
            }
            receiver_ = std::make_shared<PreviewReceiver>(previewDirectory_);

            std::shared_ptr<ins_camera::StreamDelegate> delegate = receiver_;
camera_->SetStreamDelegate(delegate);

            ins_camera::LiveStreamParam param;
            param.video_resolution = ins_camera::VideoResolution::RES_1920_960P30;
            param.lrv_video_resulution = ins_camera::VideoResolution::RES_1920_960P30;
            param.video_bitrate = 1024 * 1024 / 2;
            param.enable_audio = false;
            param.using_lrv = false;
            try {
                if (!camera_->StartLiveStreaming(param)) {
                    receiver_.reset();
                    error("StartLiveStreaming() failed. Check camera mode and SDK log.");
                    return;
                }
                previewing_ = true;
                previewStatus_->setText("Preview started. Waiting for video packets...");
                updateButtons();
            } catch (const std::exception& e) {
                receiver_.reset();
                error(QString("Preview error: ") + e.what());
            }
        });
        connect(previewStop_, &QPushButton::clicked, this, [this] { stopPreview(); });
        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this] {
            if (!previewing_ || !receiver_) return;
            previewStatus_->setText(
                QString("Receiving video: %1 packets, %2 bytes, last stream %3. Saving to %4")
                    .arg(receiver_->packets.load())
                    .arg(receiver_->bytes.load())
                    .arg(receiver_->lastStream.load())
                    .arg(previewDirectory_));
        });
        timer->start(1000);
        updateButtons();
    }
    ~X5Window() override { if (camera_) { stopPreview(); camera_->Close(); } }
};
int main(int argc, char **argv) { QApplication app(argc, argv); X5Window window; window.show(); return app.exec(); }
