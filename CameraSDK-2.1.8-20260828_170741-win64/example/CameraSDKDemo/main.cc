// CameraSDKDemo: unified entry point for all camera SDK demos.
// Modules are in separate .h files for code organization.
// 运行: CameraSDKDemo [--debug [级别]] [--log_file [path]] [--en|--cn]
//   不加 --debug            日志级别 = ERROR（仅错误）
//   --debug                 日志级别 = INFO（默认调试级别：含下载采样点、write_some 卡顿告警）
//   --debug <级别>          指定级别，<级别> ∈ {verbose|info|warning|error}（debug 等价 verbose）
//                           verbose 会额外打印每帧收发明细，刷屏量大，仅深度排查用

#include "../common/camera_setup.h"
#include "../common/i18n.h"
#include "capture_settings.h"
#include "file.h"
#include "photo.h"
#include "preview.h"
#include "status.h"
#include "system.h"
#include "video.h"
#include <iostream>

// Global state shared across all modules
std::shared_ptr<ins_camera::Camera> g_cam;
ins_camera::CameraType g_cameraType = ins_camera::CameraType::Unknown;
// Extern from photo.h / video.h
namespace photo {
extern bool g_capturing;
}
namespace video {
extern bool g_running;
}

int main(int argc, char* argv[]) {
    std::string log_file_arg;
    // 默认 ERROR；--debug 不带级别时用 INFO；--debug <级别> 用指定级别。
    ins_camera::LogLevel log_level = ins_camera::LogLevel::ERR;
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        if (a == "--debug") {
            // 后面若紧跟一个非选项 token，当作级别名解析；否则用默认 INFO
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                log_level = demo_common::parseLogLevel(argv[++i]);
            } else {
                log_level = ins_camera::LogLevel::INFO;
            }
        } else if (a == "--log_file") {
            if (i + 1 < argc && argv[i + 1][0] != '-')
                log_file_arg = argv[++i];
            else
                log_file_arg = "*";
        } else if (a == "--en")
            demo_common::lang() = demo_common::Lang::EN;
        else if (a == "--cn")
            demo_common::lang() = demo_common::Lang::CN;
    }

    g_cam = demo_common::openCamera(log_level, &g_cameraType,
                                    demo_common::resolveLogPath(log_file_arg, demo_common::exeDir(argv[0])));
    if (!g_cam)
        return -1;

    while (true) {
        std::cout << "\n========================================" << std::endl;
        std::cout << demo_common::L("  CameraSDK Demo 主菜单", "  CameraSDK Demo Menu") << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "  1: " << demo_common::L("拍照 (Photo)", "Photo") << std::endl;
        std::cout << "  2: " << demo_common::L("录像 (Video)", "Video") << std::endl;
        std::cout << "  3: " << demo_common::L("曝光与设置 (CaptureSettings)", "Capture Settings") << std::endl;
        std::cout << "  4: " << demo_common::L("运行状态 (Status)", "Status") << std::endl;
        std::cout << "  5: " << demo_common::L("文件与存储 (File)", "File") << std::endl;
        std::cout << "  6: " << demo_common::L("实时预览 (Preview)", "Preview") << std::endl;
        std::cout << "  7: " << demo_common::L("系统 (System)", "System") << std::endl;
        std::cout << "  8: " << demo_common::L("重连相机 (Reconnect)", "Reconnect Camera") << std::endl;
        std::cout << "  0: " << demo_common::L("退出", "Exit") << std::endl;
        std::cout << demo_common::L("输入", "Enter") << ": ";

        int opt = -1;
        if (!(std::cin >> opt)) {
            std::cin.clear();
            std::cin.ignore(0x7fffffff, '\n');
            continue;
        }
        if (opt == 0)
            break;

        switch (opt) {
        case 1:
            if (video::g_running) {
                std::cout << demo_common::L("录像中，请先停止", "Stop recording first") << std::endl;
                break;
            }
            photo::run();
            break;
        case 2:
            if (photo::g_capturing && g_cam->CaptureCurrentStatus()) {
                std::cout << demo_common::L("拍照延时中，请先停止", "Stop photo timelapse first") << std::endl;
                break;
            }
            video::run();
            break;
        case 3: capture_settings::run(); break;
        case 4: status::run(); break;
        case 5: file_demo::run(); break;
        case 6: preview::run(); break;
        case 7: system_demo::run(); break;
        case 8: {
            std::cout << demo_common::L("正在断开并重新检测设备...", "Disconnecting and rediscovering...") << std::endl;
            g_cam->Close();
            g_cam = demo_common::openCamera(log_level, &g_cameraType,
                                            demo_common::resolveLogPath(log_file_arg, demo_common::exeDir(argv[0])));
            break;
        }
        default: std::cout << demo_common::L("无效", "invalid") << std::endl; break;
        }
    }

    g_cam->Close();
    return 0;
}