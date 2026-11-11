// file.h — file & storage management sub-module for CameraSDKDemo
#pragma once
#include "../common/camera_setup.h"
#include "../common/file_utils.h"
#include "../common/i18n.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

extern std::shared_ptr<ins_camera::Camera> g_cam;
extern ins_camera::CameraType g_cameraType;

namespace file_demo {

using ins_camera::CardLocation;

inline void run() {
    while (true) {
        std::cout << "\n=== " << demo_common::L("文件与存储", "File & Storage") << " ===\n"
                  << "  1: " << demo_common::L("存储状态", "storage state") << "\n"
                  << "  2: " << demo_common::L("获取存储位置", "get storage location") << "\n"
                  << "  3: " << demo_common::L("设置存储位置", "set storage location") << "\n"
                  << "  4: " << demo_common::L("文件列表", "file list") << "\n"
                  << "  5: " << demo_common::L("下载文件", "download file") << "\n"
                  << "  6: " << demo_common::L("批量下载", "batch download") << "\n"
                  << "  7: " << demo_common::L("删除文件", "delete file") << "\n"
                  << "  8: " << demo_common::L("删除全部文件", "delete all files") << "\n"
                  << "  9: " << demo_common::L("文件计数", "file count") << "\n"
                  << " 10: " << demo_common::L("正在录制的文件", "recording files") << "\n"
                  << " 11: " << demo_common::L("格式化存储", "format storage") << "\n"
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
            ins_camera::StorageStatus status{};
            if (g_cam->GetStorageState(status)) {
                std::cout << "free_space: " << status.free_space << "  total_space: " << status.total_space
                          << "  state: " << status.state << "  location: " << static_cast<int>(status.location)
                          << std::endl;
            } else
                std::cerr << demo_common::L("失败", "Failed") << std::endl;
            break;
        }
        case 2: {
            if (g_cameraType < ins_camera::CameraType::Insta360X6) {
                std::cout << demo_common::L("仅 X6 及以上支持", "X6+ only") << std::endl;
                break;
            }
            auto loc = g_cam->GetStorageLocation();
            const char* name = (loc == CardLocation::STOR_CL_CAMERA_INNER) ? "internal"
                               : (loc == CardLocation::STOR_CL_CAMERA_SD)  ? "SD"
                               : (loc == CardLocation::STOR_CL_CAMERA)     ? "default"
                                                                           : "reader";
            std::cout << demo_common::L("当前", "Current") << ": " << static_cast<int>(loc) << " (" << name << ")"
                      << std::endl;
            break;
        }
        case 3: {
            if (g_cameraType < ins_camera::CameraType::Insta360X6) {
                std::cout << demo_common::L("仅 X6 及以上支持", "X6+ only") << std::endl;
                break;
            }
            int loc = 3;
            std::cout << "  STOR_CL_CAMERA_INNER [2] " << demo_common::L("内置", "internal") << "\n"
                      << "  STOR_CL_CAMERA_SD    [3] " << demo_common::L("SD卡", "SD") << "\n"
                      << demo_common::L("请输入", "enter") << ": ";
            std::cin >> loc;
            bool ok = g_cam->SetStorageLocation(static_cast<CardLocation>(loc));
            std::cout << (ok ? demo_common::L("成功", "OK") : demo_common::L("失败", "Failed")) << std::endl;
            break;
        }
        case 4: {
            for (auto& f : g_cam->GetCameraFilesList())
                std::cout << f << std::endl;
            break;
        }
        case 5: {
            auto files = g_cam->GetCameraFilesList();
            for (auto& f : files)
                std::cout << f << std::endl;
            std::string remote, local;
            std::cout << demo_common::L("远程路径", "remote path") << ": ";
            std::cin >> remote;
            if (std::find(files.begin(), files.end(), remote) == files.end()) {
                std::cout << demo_common::L("文件不存在", "not found") << std::endl;
                break;
            }
            std::cout << demo_common::L("本地路径", "local path") << ": ";
            std::cin >> local;
            std::string dir, name;
            demo_common::parsePath(local, dir, name);
            if (!demo_common::fileIsExist(dir)) {
                std::cout << demo_common::L("目录不存在", "dir not found") << ": " << dir << std::endl;
                break;
            }
            if (name.empty()) {
                std::cout << demo_common::L("需要文件名", "need filename") << std::endl;
                break;
            }
            demo_common::downloadFileFromCamera(g_cam, remote, local);
            break;
        }
        case 6: {
            auto files = g_cam->GetCameraFilesList();
            if (files.empty()) {
                std::cout << demo_common::L("无文件", "no files") << std::endl;
                break;
            }
            std::string dir;
            std::cout << demo_common::L("下载目录", "download dir") << ": ";
            std::cin >> dir;
            std::replace(dir.begin(), dir.end(), '\\', '/');
            if (!demo_common::endsWithSlash(dir))
                dir += "/";
            for (auto& url : files) {
                std::cout << demo_common::L("下载", "Download") << ": " << url << std::endl;
                demo_common::downloadFileFromCamera(g_cam, url, dir + demo_common::GetFileName(url));
            }
            break;
        }
        case 7: {
            auto files = g_cam->GetCameraFilesList();
            for (auto& f : files)
                std::cout << f << std::endl;
            std::string target;
            std::cout << demo_common::L("删除文件", "delete file") << ": ";
            std::cin >> target;
            if (target.empty() || std::find(files.begin(), files.end(), target) == files.end()) {
                std::cout << demo_common::L("文件不存在", "not found") << std::endl;
                break;
            }
            std::cout << (g_cam->DeleteCameraFile(target) ? demo_common::L("删除成功", "deleted")
                                                          : demo_common::L("删除失败", "failed"))
                      << std::endl;
            break;
        }
        case 8: {
            for (auto& f : g_cam->GetCameraFilesList()) {
                std::cout << (g_cam->DeleteCameraFile(f) ? demo_common::L("已删除", "deleted")
                                                         : demo_common::L("失败", "failed"))
                          << " " << f << std::endl;
            }
            break;
        }
        case 9: {
            int count = 0;
            if (g_cam->GetCameraFilesCount(count))
                std::cout << demo_common::L("文件数", "count") << ": " << count << std::endl;
            else
                std::cout << demo_common::L("失败", "Failed") << std::endl;
            break;
        }
        case 10: {
            std::vector<std::string> list;
            if (g_cam->GetRecordingFiles(list))
                for (auto& f : list)
                    std::cout << f << std::endl;
            else
                std::cerr << demo_common::L("失败", "Failed") << std::endl;
            break;
        }
        case 11: {
            int loc = static_cast<int>(CardLocation::STOR_CL_AUTO);
            if (g_cameraType >= ins_camera::CameraType::Insta360X6) {
                std::cout << "  AUTO[99]  INNER[2]  SD[3]\n" << demo_common::L("请输入", "enter") << ": ";
                std::cin >> loc;
                if (loc != 99 && loc != 2 && loc != 3) {
                    std::cout << demo_common::L("无效,取消", "invalid,cancelled") << std::endl;
                    break;
                }
            }
            std::cout << demo_common::L("警告:将擦除全部数据! 确认(y/n)", "WARNING: erase all data! Confirm (y/n)?")
                      << ": ";
            char c;
            std::cin >> c;
            if (c != 'y' && c != 'Y') {
                std::cout << demo_common::L("已取消", "cancelled") << std::endl;
                break;
            }
            std::cout << demo_common::L("格式化中...", "Formatting...") << std::endl;
            bool ok = g_cam->FormatStorage(static_cast<CardLocation>(loc));
            std::cout << (ok ? demo_common::L("格式化命令已发送", "Format command sent")
                             : demo_common::L("失败", "Failed"))
                      << std::endl;
            break;
        }
        default: std::cout << demo_common::L("无效", "invalid") << std::endl; break;
        }
    }
}

} // namespace file_demo