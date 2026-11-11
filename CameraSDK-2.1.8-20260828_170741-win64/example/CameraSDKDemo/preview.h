// preview.h — live preview sub-module for CameraSDKDemo
#pragma once
#include "../common/camera_setup.h"
#include "../common/i18n.h"
#include <camera/camera.h>
#include <camera/photography_settings.h>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <stream/stream_delegate.h>
#include <string>
#include <thread>

extern std::shared_ptr<ins_camera::Camera> g_cam;

namespace preview {

static std::shared_ptr<ins_camera::StreamDelegate> g_delegate;
static bool g_previewing = false;

class FileStreamDelegate : public ins_camera::StreamDelegate {
public:
    void OnVideoData(const uint8_t* data, size_t size, int64_t timestamp, uint8_t, int stream_index) override {
        if (!file_.is_open()) {
            std::string name = "preview_stream_" + std::to_string(stream_index) + ".h264";
            file_.open(name, std::ios::binary | std::ios::trunc);
            std::cout << demo_common::L("录制到", "Recording to") << ": " << name << std::endl;
        }
        file_.write(reinterpret_cast<const char*>(data), size);
    }
    void OnAudioData(const uint8_t*, size_t, int64_t) override {}
    void OnGyroData(const std::vector<ins_camera::GyroData>&) override {}
    void OnExposureData(const ins_camera::ExposureData&) override {}
    ~FileStreamDelegate() {
        if (file_.is_open())
            file_.close();
    }

private:
    std::ofstream file_;
};

inline void run() {
    while (true) {
        std::cout << "\n=== " << demo_common::L("实时预览", "Preview") << " ===\n"
                  << "  1: " << demo_common::L("开始预览", "start preview") << "\n"
                  << "  2: " << demo_common::L("停止预览", "stop preview") << "\n"
                  << "  3: " << demo_common::L("预览+录像", "preview+record") << "\n"
                  << "  4: " << demo_common::L("停止预览+录像", "stop preview+record") << "\n"
                  << "  0: " << demo_common::L("返回", "back") << "\n"
                  << demo_common::L("输入", "enter") << ": ";
        int opt = -1;
        if (!(std::cin >> opt)) {
            std::cin.clear();
            std::cin.ignore(0x7fffffff, '\n');
            continue;
        }
        if (opt == 0) {
            if (g_previewing) {
                g_cam->StopLiveStreaming();
                g_previewing = false;
            }
            break;
        }
        switch (opt) {
        case 1: {
            if (g_previewing) {
                std::cout << demo_common::L("已在预览中", "already previewing") << std::endl;
                break;
            }
            g_delegate = std::make_shared<FileStreamDelegate>();
            g_cam->SetStreamDelegate(g_delegate);
            ins_camera::LiveStreamParam param;
            param.video_resolution = ins_camera::VideoResolution::RES_1920_960P30;
            param.lrv_video_resulution = ins_camera::VideoResolution::RES_1920_960P30;
            param.video_bitrate = 1024 * 1024 / 2;
            param.enable_audio = false;
            param.using_lrv = false;
            if (g_cam->StartLiveStreaming(param)) {
                g_previewing = true;
                std::cout << demo_common::L("预览已开启", "Preview started") << std::endl;
            } else
                std::cout << demo_common::L("失败", "Failed") << std::endl;
            break;
        }
        case 2: {
            if (!g_previewing) {
                std::cout << demo_common::L("未在预览", "not previewing") << std::endl;
                break;
            }
            g_cam->StopLiveStreaming();
            g_previewing = false;
            std::cout << demo_common::L("已停止", "Stopped") << std::endl;
            break;
        }
        case 3: {
            if (g_previewing) {
                std::cout << demo_common::L("已在预览中", "already previewing") << std::endl;
                break;
            }
            g_delegate = std::make_shared<FileStreamDelegate>();
            g_cam->SetStreamDelegate(g_delegate);
            ins_camera::LiveStreamParam param;
            param.video_resolution = ins_camera::VideoResolution::RES_1920_960P30;
            param.lrv_video_resulution = ins_camera::VideoResolution::RES_1920_960P30;
            param.video_bitrate = 1024 * 1024 / 2;
            param.enable_audio = false;
            param.using_lrv = false;
            if (g_cam->StartLiveStreaming(param)) {
                g_previewing = true;
                g_cam->StartRecording();
                std::cout << demo_common::L("预览+录像已开启", "Preview+Record started") << std::endl;
            } else
                std::cout << demo_common::L("失败", "Failed") << std::endl;
            break;
        }
        case 4: {
            if (!g_previewing) {
                std::cout << demo_common::L("未在预览", "not previewing") << std::endl;
                break;
            }
            auto url = g_cam->StopRecording();
            g_cam->StopLiveStreaming();
            g_previewing = false;
            if (!url.Empty())
                std::cout << demo_common::L("录像文件", "Recorded") << ": " << url.GetSingleOrigin() << std::endl;
            std::cout << demo_common::L("已停止", "Stopped") << std::endl;
            break;
        }
        default: std::cout << demo_common::L("无效", "invalid") << std::endl; break;
        }
    }
}

} // namespace preview