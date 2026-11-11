// =============================================================================
// RealTimeStitcherSDKTest — Insta360 MediaSDK Real-Time Stitching Demo
//
// Overview:
//   This program connects to an Insta360 camera, pulls per-frame video,
//   gyroscope, and exposure data from the camera's preview stream, and feeds
//   that data to MediaSDK's RealTimeStitcher for live 360-degree panoramic
//   stitching. The stitched output is displayed in real time via an OpenCV
//   window.
//
// Dependencies:
//   - InsMediaSDK : Stitching core. Key header: ins_realtime_stitcher.h.
//   - Insta360 CameraSDK : Discovers, opens the camera, and delivers data
//     streams via the StreamDelegate callback interface.
//   - OpenCV : Used for on-screen display of the stitched frames.
//
// How to Run:
//   1) Connect an Insta360 camera via USB or Wi-Fi.
//   2) Launch RealTimeStitcherSDKTest. The program will enumerate available
//      cameras automatically and open the first one found.
//   3) Use the interactive prompt:
//        1  — start preview / stitching
//        2  — stop  preview / stitching
//        0  — quit
//   4) Optional command-line flags:
//        --debug              Enable verbose CameraSDK logging for debugging.
//        --log_file <path>    Write camera log output to the specified file.
//
// Key Usage Flow:
//   InitEnv
//     → Enumerate cameras with DeviceDiscovery
//     → Open the camera (Camera::Open)
//     → Create RealTimeStitcher and push CameraInfo (model, codec, crop)
//     → Implement StreamDelegate to forward video / gyro / exposure data
//       to the stitcher's Handle* methods
//     → Register a stitched-frame callback (SetStitchRealTimeDataCallback)
//     → Start the camera preview stream (StartLiveStreaming)
//     → Start the stitcher (StartStitch)
//     → Display loop driven by the frame callback
//     → StopLiveStreaming → CancelStitch → Camera::Close on exit
// =============================================================================

#include <iostream>
#include <ins_realtime_stitcher.h>
#include <ins_stitcher.h>   // ins::SetModelFileRootDir is declared here
#include <camera/camera.h>
#include <camera/device_discovery.h>

#include <iostream>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <chrono>
#include <vector>
#include <sstream>
#include <opencv2/opencv.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

const std::string window_name = "realtime_stitcher";

// Return the directory containing the executable (with a trailing separator),
// used to resolve the default models/ directory next to the exe so that
// realtime stitching (AIFLOW) can locate ai_stitcher.ins without setting a
// working directory.  Same implementation as in main.cc.
std::string getExecutableDir() {
    std::string dir;
#ifdef _WIN32
    char buffer[1024];
    GetModuleFileNameA(NULL, buffer, sizeof(buffer));
    std::string path = buffer;
    dir = path.substr(0, path.find_last_of("\\") + 1);
#else
    char buffer[4096];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len > 0) {
        buffer[len] = '\0';
        std::string path = buffer;
        dir = path.substr(0, path.find_last_of('/') + 1);
    }
#endif
    return dir;
}

// Split a string by a single-character delimiter and return the tokens.
// Used here to parse comma-separated lists that may appear in camera metadata.
std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);

    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

// StitchDelegate — bridges CameraSDK data callbacks to the MediaSDK stitcher.
//
// CameraSDK delivers camera-generated data (video frames, gyroscope samples,
// exposure values, and audio) to the application layer through the
// StreamDelegate interface. This implementation forwards each data type to
// the corresponding RealTimeStitcher Handle* method so that stitching happens
// transparently inside the SDK.
//
// Audio is not required for real-time stitching, so OnAudioData is left as a
// no-op.
class StitchDelegate : public ins_camera::StreamDelegate {
public:
    // Construct a delegate that forwards data to the provided stitcher instance.
    StitchDelegate(const std::shared_ptr<ins::RealTimeStitcher>& stitcher) :stitcher_(stitcher) {
    }

    virtual ~StitchDelegate() {
    }

    // Audio data callback — not needed for stitching; intentionally empty.
    void OnAudioData(const uint8_t* data, size_t size, int64_t timestamp) override {}

    // Video frame callback.
    // Forwards raw compressed video data from the camera to the stitcher.
    // streamType identifies the encoding format; stream_index identifies which
    // lens stream the data belongs to (relevant for multi-lens cameras).
    void OnVideoData(const uint8_t* data, size_t size, int64_t timestamp, uint8_t streamType, int stream_index) override {
        stitcher_->HandleVideoData(data, size, timestamp, streamType, stream_index);
    }

    // Gyroscope data callback.
    // Gyro samples are required by the FlowState stabilisation algorithm inside
    // the stitcher. The CameraSDK GyroData layout is binary-compatible with the
    // MediaSDK GyroData struct, so a direct memcpy is safe here.
    void OnGyroData(const std::vector<ins_camera::GyroData>& data) override {
        std::vector<ins::GyroData> data_vec(data.size());
        memcpy(data_vec.data(), data.data(), data.size() * sizeof(ins_camera::GyroData));
        stitcher_->HandleGyroData(data_vec);
    }

    // Exposure data callback.
    // Exposure time and timestamp are forwarded to the stitcher for colour and
    // brightness alignment between the front and rear lenses.
    void OnExposureData(const ins_camera::ExposureData& data) override {
        ins::ExposureData exposure_data{};
        exposure_data.exposure_time = data.exposure_time;
        exposure_data.timestamp = data.timestamp;
        stitcher_->HandleExposureData(exposure_data);
    }

private:
    std::shared_ptr<ins::RealTimeStitcher> stitcher_;
};

int main(int argc, char* argv[]) {
    // Initialize the MediaSDK runtime. This must be called before any other
    // SDK function.
    ins::InitEnv();
    std::cout << "begin open camera" << std::endl;

    // Set the AI model root directory to models/ next to the exe (trailing separator).
    // Realtime stitching uses AIFLOW (see SetStitchType below), which needs to load
    // ai_stitcher.ins; if unset, model_root_dir_ is empty and models are looked up
    // relative to cwd, usually not found and silently degraded.
#ifdef _WIN32
    ins::SetModelFileRootDir(getExecutableDir() + "models\\");
#else
    ins::SetModelFileRootDir(getExecutableDir() + "models/");
#endif

    // Default log level is WARNING to avoid flooding the console. Pass --debug
    // on the command line to switch to VERBOSE for troubleshooting.
    ins_camera::SetLogLevel(ins_camera::LogLevel::WARNING);
    ins::SetLogLevel(ins::InsLogLevel::WARNING);
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == std::string("--debug")) {
            // Enable verbose logging for both CameraSDK and MediaSDK.
            ins_camera::SetLogLevel(ins_camera::LogLevel::VERBOSE);
        }
        else if (arg == std::string("--log_file")) {
            // Redirect CameraSDK log output to the specified file path.
            const std::string log_file = argv[++i];
            ins_camera::SetLogPath(log_file);
        }
    }

    // Enumerate all Insta360 cameras currently reachable over USB or Wi-Fi.
    // An empty list means no camera was detected, the camera is powered off, or
    // it is already claimed by another process.
    ins_camera::DeviceDiscovery discovery;
    auto list = discovery.GetAvailableDevices();
    if (list.size() <= 0) {
        std::cerr << "no device found." << std::endl;
        discovery.FreeDeviceDescriptors(list);
        return -1;
    }

    // Print basic information for every discovered camera.
    for (const auto& camera : list) {
        std::cout << "serial:" << camera.serial_number << "\t"
            << ";camera type:" << camera.camera_name << "\t"
            << ";fw version:" << camera.fw_version << "\t"
            << std::endl;
    }

    // Select the first camera in the list. To target a specific device, filter
    // the list by serial_number before calling Camera::Open.
    auto cam = std::make_shared<ins_camera::Camera>(list[0].info);
    if (!cam->Open()) {
        std::cerr << "failed to open camera" << std::endl;
        return -1;
    }

    const auto serial_number = list[0].serial_number;

    // Release the device descriptor list now that the camera is open.
    discovery.FreeDeviceDescriptors(list);

    // Shared state used by the display thread:
    //   show_image_       — the latest stitched frame (RGBA).
    //   show_image_mutex_ — guards show_image_ and is_stop_.
    //   show_image_cond_  — signals the display thread when a new frame arrives
    //                       or when it should stop.
    //   is_stop_          — true when the display thread should exit.
    cv::Mat show_image_;
    std::thread show_thread_;
    std::mutex show_image_mutex_;
    bool is_stop_ = true;
    std::condition_variable show_image_cond_;

    // Create the real-time stitcher and populate CameraInfo from the camera's
    // preview parameters. CameraInfo tells the stitcher which camera model is
    // in use, the video codec, lens calibration offsets, and cropping geometry.
    // These values drive the stitcher's internal decode and geometry-correction
    // pipeline.
    std::shared_ptr<ins::RealTimeStitcher> stitcher = std::make_shared<ins::RealTimeStitcher>();
    // Build CameraInfo from the camera's preview parameters.
    // CameraSDK hides the internal calibration/crop layout behind accessors,
    // so no version-specific names leak into the SDK API or into demo code.
    auto preview = cam->GetPreviewParam();
    ins::CameraInfo info;
    info.cameraName  = preview.camera_name;
    info.decode_type = static_cast<ins::VideoDecodeType>(preview.encode_type);
    {
        std::vector<std::string> calib;
        for (size_t i = 0; i < preview.GetCalibrationCount(); ++i)
            calib.push_back(preview.GetCalibration(i));
        info.SetCalibration(calib,
            preview.GetCropSrcWidth(), preview.GetCropSrcHeight(),
            preview.GetCropDstWidth(), preview.GetCropDstHeight(),
            preview.GetCropOffsetX(), preview.GetCropOffsetY());
    }
    stitcher->SetCameraInfo(info);

    // DYNAMICSTITCH is recommended for real-time use: it balances speed and
    // quality well. TEMPLATE is faster but lower quality; AIFLOW is the highest
    // quality but requires GPU/model support and has higher latency.
    stitcher->SetStitchType(ins::STITCH_TYPE::DYNAMICSTITCH);

    // Enable gyroscope-driven stabilisation (FlowState). Requires gyro data to
    // be forwarded via StitchDelegate::OnGyroData.
    stitcher->EnableFlowState(true);

    // Set the stitched output resolution. Larger values increase quality but
    // also increase processing time and display latency.
    stitcher->SetOutputSize(960, 480);

    // Register the stitched-frame callback. This lambda is invoked on an SDK
    // internal thread each time a new panoramic frame is ready.
    //   data[0]    — pointer to the RGBA pixel buffer.
    //   linesize[0]— row stride in bytes.
    // The frame buffer is reused by the SDK after the callback returns, so the
    // cv::Mat must be cloned before being handed off to the display thread to
    // avoid tearing caused by buffer reuse.
    stitcher->SetStitchRealTimeDataCallback([&](uint8_t* data[4], int linesize[4], int width, int height, int format, int64_t timestamp) {
        std::unique_lock<std::mutex> lck(show_image_mutex_);
        show_image_ = cv::Mat(height, width, CV_8UC4, data[0]).clone();
        show_image_cond_.notify_one();
    });

    // Wire the camera data stream to the stitcher by installing the delegate.
    // From this point on, all video/gyro/exposure callbacks from CameraSDK will
    // be forwarded automatically to the stitcher.
    std::shared_ptr<ins_camera::StreamDelegate> delegate = std::make_shared<StitchDelegate>(stitcher);
    cam->SetStreamDelegate(delegate);

    std::cout << "Succeed to open camera..." << std::endl;

    std::cout << "Usage:" << std::endl;
    std::cout << "1: start preview live streaming:" << std::endl;
    std::cout << "2: stop preview live streaming:" << std::endl;

    // Main input loop — runs until the user enters 0 to quit.
    int option = 0;
    while (true) {
        std::cout << "please enter index: ";
        std::cin >> option;
        if (option < 0 || option > 39) {
            std::cout << "Invalid index" << std::endl;
            continue;
        }

        // 0 — exit the loop and begin the shutdown sequence.
        if (option == 0) {
            break;
        }

        if (option == 1) {
            // Ignore a redundant start request if preview is already running.
            if (!is_stop_) {
                std::cout << "" << std::endl;
                continue;
            }

            // Configure the camera preview stream parameters: resolution,
            // bitrate, and whether audio and the LRV (low-resolution video)
            // secondary stream are enabled.
            ins_camera::LiveStreamParam param;
            param.video_resolution = ins_camera::VideoResolution::RES_1440_720P30;
            param.lrv_video_resulution = ins_camera::VideoResolution::RES_1440_720P30;
            param.video_bitrate = 1024 * 1024 / 2;
            param.enable_audio = false;
            param.using_lrv = false;

            // Start the camera preview stream first, then start the stitcher.
            // Reversing this order would cause the first batch of frames to be
            // dropped before the stitcher is ready to receive them.
            if (cam->StartLiveStreaming(param)) {
                stitcher->StartStitch();
                std::cout << "successfully started live stream" << std::endl;
            }

            // Launch the display thread. It waits on show_image_cond_ for new
            // stitched frames, converts the colour space from RGBA to BGRA
            // (as required by OpenCV), and refreshes the named window.
            show_thread_ = std::thread([&]() {
                cv::namedWindow(window_name, cv::WINDOW_NORMAL);
                is_stop_ = false;
                while (!is_stop_)
                {
                    std::unique_lock<std::mutex> lck(show_image_mutex_);
                    // Block until a new frame is available or a stop is requested.
                    show_image_cond_.wait(lck, [&]() {
                        return  is_stop_ || !show_image_.empty();
                    });

                    if (is_stop_) {
                        break;
                    }

                    // Grab the frame under the lock, then release the lock
                    // before the (potentially slow) colour conversion and imshow.
                    auto temp = show_image_.clone();
                    show_image_ = cv::Mat();
                    lck.unlock();
                    cv::cvtColor(temp, temp, cv::COLOR_RGBA2BGRA);
                    cv::imshow(window_name, temp);
                    cv::waitKey(5);
                }
            });
        }

        if (option == 2) {
            // Shutdown sequence for option 2 (stop while staying in the loop):
            //   1. Signal the display thread to exit via is_stop_ and the
            //      condition variable, then join it.
            //   2. Destroy the OpenCV window.
            //   3. Stop the camera live stream, then cancel the stitcher.
            //
            // This order is important: stopping the camera first would leave the
            // display thread blocked on the condition variable indefinitely
            // because no more frame callbacks would arrive to wake it.
            std::unique_lock<std::mutex> lck(show_image_mutex_);
            is_stop_ = true;
            show_image_cond_.notify_one();
            lck.unlock();
            if (show_thread_.joinable()) {
                show_thread_.join();
            }
            cv::destroyWindow(window_name);
            if (cam->StopLiveStreaming()) {
                stitcher->CancelStitch();
                std::cout << "success!" << std::endl;
            }
            else {
                std::cerr << "failed to stop live." << std::endl;
            }
        }
    }

    // Final cleanup on exit (option == 0). Mirrors the option 2 shutdown
    // sequence to ensure all resources are released regardless of whether the
    // user stopped the stream manually before quitting.
    std::unique_lock<std::mutex> lck(show_image_mutex_);
    is_stop_ = true;
    show_image_cond_.notify_one();
    lck.unlock();
    if (show_thread_.joinable()) {
        show_thread_.join();
    }
    cv::destroyWindow(window_name);
    cam->Close();
    return 0;
}
