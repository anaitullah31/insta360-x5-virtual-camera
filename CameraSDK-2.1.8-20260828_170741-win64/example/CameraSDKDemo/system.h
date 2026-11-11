// system.h — system sub-module for CameraSDKDemo
#pragma once
#include "../common/camera_setup.h"
#include "../common/file_utils.h"
#include "../common/i18n.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <string>

extern std::shared_ptr<ins_camera::Camera> g_cam;
extern ins_camera::CameraType g_cameraType;

namespace system_demo {

inline void run() {
    while (true) {
        std::cout << "\n=== " << demo_common::L("系统", "System") << " ===\n"
                  << "  1: " << demo_common::L("上传固件", "upload firmware") << "\n"
                  << "  2: " << demo_common::L("下载相机日志", "download camera log") << "\n"
                  << "  3: " << demo_common::L("关机", "shutdown") << "\n"
                  << "  4: " << demo_common::L("断连自动停录", "auto-stop on disconnect") << "\n"
                  << "  5: " << demo_common::L("同步时间", "sync time") << "\n"
                  << "  0: " << demo_common::L("返回", "back") << "\n"
                  << demo_common::L("输入", "enter") << ": ";
        int opt = -1;
        if (!(std::cin >> opt)) {
            std::cin.clear();
            std::cin.ignore(0x7fffffff, '\n');
            continue;
        }
        if (opt == 0)
            break;

        switch (opt) {
        case 1: {
            if (g_cameraType < ins_camera::CameraType::Insta360X4) {
                std::cout << demo_common::L("仅 X4 及以上支持", "X4+ only") << std::endl;
                break;
            }
            std::string local_path;
            std::cout << demo_common::L("固件路径", "firmware path") << ": ";
            std::cin >> local_path;
            if (local_path.empty() || !demo_common::fileIsExist(local_path)) {
                std::cout << demo_common::L("文件不存在", "not found") << std::endl;
                break;
            }
            std::string file_name = "Insta360X4FW.bin";
            if (g_cameraType == ins_camera::CameraType::Insta360X5)
                file_name = "Insta360X5FW.bin";
            if (g_cameraType == ins_camera::CameraType::Insta360X4Air)
                file_name = "Insta360X4AirFW.bin";
            if (g_cameraType == ins_camera::CameraType::Insta360X6) {
                file_name = local_path.substr(local_path.find_last_of("/\\") + 1);
            }
            int64_t progress = -1;
            auto ret = g_cam->UploadFile(local_path, file_name, [&](int64_t total, int64_t uploaded) {
                int64_t p = uploaded * 100 / total;
                if (progress != p) {
                    progress = p;
                    std::cout << "\r" << p << "%" << std::flush;
                }
            });
            std::cout << std::endl
                      << (ret ? demo_common::L("上传成功", "OK") : demo_common::L("失败", "Failed")) << std::endl;
            if (ret) {
                std::cout << demo_common::L("请重启相机以完成升级", "Restart camera to complete update") << std::endl;
            }
            break;
        }
        case 2: {
            auto log_url = g_cam->GetCameraLogFileUrl();
            if (log_url.empty()) {
                std::cout << demo_common::L("获取日志URL失败", "log URL failed") << std::endl;
                break;
            }
            std::cout << "URL: " << log_url << std::endl;
            std::string dir;
            std::cout << demo_common::L("下载目录", "download dir") << ": ";
            std::cin >> dir;
            std::replace(dir.begin(), dir.end(), '\\', '/');
            if (!demo_common::fileIsExist(dir)) {
                std::cout << demo_common::L("目录不存在", "not found") << std::endl;
                break;
            }
            std::string log_dir, log_name;
            demo_common::parsePath(log_url, log_dir, log_name);
            if (!demo_common::endsWithSlash(dir))
                dir += "/";
            demo_common::downloadFileFromCamera(g_cam, log_url, dir + log_name);
            break;
        }
        case 3: {
            bool ok = g_cam->ShutdownCamera();
            std::cout << (ok ? demo_common::L("关机命令已发送(X5支持, X6固件暂不支持)",
                                              "Shutdown sent (X5; X6 fw not yet)")
                             : demo_common::L("失败", "Failed"))
                      << std::endl;
            break;
        }
        case 4: {
            // 该功能暂时无效
            std::cout << demo_common::L("该功能暂时无效", "This feature is temporarily unavailable") << std::endl;
            if (g_cameraType < ins_camera::CameraType::Insta360X4) {
                std::cout << demo_common::L("仅 X4 及以上支持", "X4+ only") << std::endl;
                break;
            }
            int enable = 0;
            std::cout << demo_common::L("启用(1)/禁用(0)", "Enable(1)/Disable(0)?") << ": ";
            std::cin >> enable;
            bool ok = g_cam->EnableAutoStopRecordAfterDisconnet(enable ? true : false);
            std::cout << (ok ? demo_common::L("成功", "OK") : demo_common::L("失败", "Failed")) << std::endl;
            break;
        }
        case 5: {
            time_t now = time(nullptr);
            std::tm tm{};
            time_t time_seconds = now;
#ifdef WIN32
            localtime_s(&tm, &now);
            time_seconds = _mkgmtime(&tm);
#else
            localtime_r(&now, &tm);
            time_seconds = timegm(&tm);
#endif
            std::cout << "UTC: " << now << "  GMT: " << time_seconds << std::endl;
            bool ok =
                g_cam->SyncLocalTimeToCamera(static_cast<uint64_t>(now), static_cast<int32_t>(time_seconds - now));
            std::cout << (ok ? demo_common::L("同步成功", "OK") : demo_common::L("失败", "Failed")) << std::endl;
            break;
        }
        default: std::cout << demo_common::L("无效", "invalid") << std::endl; break;
        }
    }
}

} // namespace system_demo