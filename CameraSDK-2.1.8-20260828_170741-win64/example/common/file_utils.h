// Shared file/path utilities: ported from CameraSDKTest/main.cc as inline functions, reused by the File/Photo/System
// demos.
#pragma once
#include <camera/camera.h>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#define ACCESS_FUNC _access
#define F_OK 0
#else
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#define ACCESS_FUNC access
#endif

namespace demo_common {

inline bool fileIsExist(const std::string& file_path) {
    return ACCESS_FUNC(file_path.c_str(), F_OK) == 0;
}

inline std::string getExecutableDir() {
    char path[1024] = {0};
#ifdef _WIN32
    const DWORD len = GetModuleFileNameA(nullptr, path, sizeof(path));
    if (len == 0 || len >= sizeof(path))
        return "./";
#else
    const ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len <= 0 || static_cast<size_t>(len) >= sizeof(path))
        return "./";
    path[len] = '\0';
#endif
    const std::string full(path);
    const size_t pos = full.find_last_of("/\\");
    if (pos == std::string::npos)
        return "./";
    return full.substr(0, pos + 1);
}

inline bool ensureDir(const std::string& dir) {
    if (dir.empty() || fileIsExist(dir))
        return true;
#ifdef _WIN32
    return _mkdir(dir.c_str()) == 0;
#else
    return mkdir(dir.c_str(), 0755) == 0;
#endif
}

inline bool endsWithSlash(const std::string& path) {
    if (path.empty())
        return false;
    return path.back() == '/' || path.back() == '\\';
}

inline void parsePath(const std::string& path, std::string& dir, std::string& filename) {
    const size_t lastSlash = path.find_last_of("/\\");
    if (lastSlash == std::string::npos) {
        dir.clear();
        filename = path;
    } else {
        dir = path.substr(0, lastSlash + 1);
        filename = path.substr(lastSlash + 1);
    }
}

inline std::string GetFileName(const std::string& path) {
    std::smatch sm;
    std::regex_match(path, sm, std::regex("(.+?)([^\\/\\\\]+$)"));
    if (sm.size() <= 2)
        return path;
    return sm[2].str();
}

inline bool downloadFileFromCamera(const std::shared_ptr<ins_camera::Camera>& camera, const std::string& src_url,
                                   const std::string& download_url) {
    std::string dst_dir;
    std::string dst_filename;
    parsePath(download_url, dst_dir, dst_filename);
    if (!dst_dir.empty() && !fileIsExist(dst_dir)) {
        std::cout << "Download " << src_url << " failed!!! download dir not exist: " << dst_dir << std::endl;
        return false;
    }
    int64_t current_process = -1;
    int64_t file_total_size = 0;
    const auto start_time = std::chrono::steady_clock::now();
    const auto ret = camera->DownloadCameraFile(src_url, download_url, [&](int64_t current, int64_t total_size) {
        file_total_size = total_size;
        const int64_t process = current * 100 / total_size;
        if (process != current_process) {
            current_process = process;
            std::cout << "\rprocess = " << process << "%" << std::flush;
        }
    });
    const auto end_time = std::chrono::steady_clock::now();
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    std::cout << std::endl;
    if (ret) {
        std::string size_str;
        if (file_total_size < 1024 * 1024) {
            size_str = std::to_string(file_total_size / 1024) + " KB";
        } else {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1) << (file_total_size / (1024.0 * 1024.0)) << " MB";
            size_str = oss.str();
        }
        std::cout << "Download " << src_url << " succeed!!!" << std::endl;
        std::cout << "  Size: " << size_str << ", Time: " << (elapsed_ms / 1000.0) << "s, Speed: ";
        if (elapsed_ms > 0) {
            const double speed_mbps = (file_total_size / (1024.0 * 1024.0)) / (elapsed_ms / 1000.0);
            std::cout << std::fixed << std::setprecision(1) << speed_mbps << " MB/s";
        }
        std::cout << std::endl;
    } else {
        std::cout << "Download " << src_url << " failed!!!" << std::endl;
    }
    return ret;
}

} // namespace demo_common
