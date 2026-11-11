// status.h — runtime status sub-module for CameraSDKDemo
#pragma once
#include "../common/camera_setup.h"
#include "../common/i18n.h"
#include <iostream>

extern std::shared_ptr<ins_camera::Camera> g_cam;

namespace status {

inline void run() {
    while (true) {
        std::cout << "\n=== " << demo_common::L("运行状态", "Status") << " ===\n"
                  << "  1: " << demo_common::L("电池状态", "Battery") << "\n"
                  << "  2: " << demo_common::L("拍摄状态", "Capture state") << "\n"
                  << "  3: " << demo_common::L("连接状态", "Connection") << "\n"
                  << "  4: " << demo_common::L("媒体时间", "Media time") << "\n"
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
            ins_camera::BatteryStatus status;
            if (g_cam->GetBatteryStatus(status))
                std::cout << demo_common::L("电量", "Battery") << ": " << status.battery_level << "%"
                          << "  type=" << (status.power_type == ins_camera::ADAPTER ? "ADAPTER" : "BATTERY")
                          << std::endl;
            else
                std::cout << demo_common::L("获取失败", "Failed") << std::endl;
            break;
        }
        case 2: {
            bool capturing = g_cam->CaptureCurrentStatus();
            std::cout << demo_common::L("拍摄状态", "Capture") << ": " << (capturing ? "YES" : "NO") << std::endl;
            break;
        }
        case 3: {
            std::cout << demo_common::L("连接", "Connected") << ": " << (g_cam->IsConnected() ? "YES" : "NO")
                      << std::endl;
            break;
        }
        case 4: {
            auto t = g_cam->GetCameraMediaTime();
            std::cout << demo_common::L("媒体时间", "Media time") << ": " << t << "ms" << std::endl;
            break;
        }
        default: std::cout << demo_common::L("无效", "invalid") << std::endl; break;
        }
    }
}

} // namespace status