// Shared boilerplate: discover and open a camera + a null StreamDelegate + a generic list-picker helper.
// Each category demo reuses this via #include "../common/camera_setup.h".
#pragma once
#include "i18n.h"
#include <camera/camera.h>
#include <camera/device_discovery.h>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#ifdef WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace demo_common {

// Null StreamDelegate: demos other than PreviewDemo don't use the preview stream.
// The virtual signatures follow StreamDelegate in include/stream/stream_delegate.h;
// consistent with TestStreamDelegate at test/CameraSDKTest/main.cc:196.
class NullStreamDelegate : public ins_camera::StreamDelegate {
public:
    void OnAudioData(const uint8_t* data, size_t size, int64_t timestamp) override {}
    void OnVideoData(const uint8_t* data, size_t size, int64_t timestamp, uint8_t streamType,
                     int stream_index) override {}
    void OnGyroData(const std::vector<ins_camera::GyroData>& data) override {}
    void OnExposureData(const ins_camera::ExposureData& data) override {}
};

// Extract directory from an executable path (argv[0]).
inline std::string exeDir(const char* argv0) {
    std::string path = argv0;
    auto pos = path.find_last_of("/\\");
    return pos == std::string::npos ? "" : path.substr(0, pos);
}

// Resolve --log_file argument into an actual path.
// - ""       → no log file (returns empty)
// - "*"      → --log_file without value: auto-generate <exe_dir>/logs/<YYYYMMDD_HHMMSS>.log
// - "path"   → use as-is, creating parent directory if needed
inline std::string resolveLogPath(const std::string& arg, const std::string& exe_dir = "") {
    if (arg.empty())
        return "";
    std::string path = arg;
    if (path == "*") {
        time_t now = time(nullptr);
        char buf[64];
        struct tm t;
#ifdef WIN32
        localtime_s(&t, &now);
#else
        localtime_r(&now, &t);
#endif
        strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &t);
        path = (exe_dir.empty() ? "" : exe_dir + "/") + std::string("logs/") + buf + ".log";
    }
    // ensure parent directory exists
    auto pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        std::string dir = path.substr(0, pos);
#ifdef WIN32
        _mkdir(dir.c_str());
#else
        mkdir(dir.c_str(), 0755);
#endif
    }
    return path;
}

// 解析日志级别名（大小写不敏感）为 ins_camera::LogLevel。
// llog 实际只有四级：verbose / info / warning / error。
// 兼容别名："debug" 视为最详细的 verbose；无法识别时回退 INFO 并给出提示。
inline ins_camera::LogLevel parseLogLevel(const std::string& name) {
    std::string s;
    s.reserve(name.size());
    for (char c : name)
        s += static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    if (s == "verbose" || s == "debug" || s == "v")
        return ins_camera::LogLevel::VERBOSE;
    if (s == "info" || s == "i")
        return ins_camera::LogLevel::INFO;
    if (s == "warning" || s == "warn" || s == "w")
        return ins_camera::LogLevel::WARNING;
    if (s == "error" || s == "err" || s == "e")
        return ins_camera::LogLevel::ERR;
    std::cerr << "unknown log level '" << name << "', fallback to INFO "
              << "(valid: verbose|info|warning|error)" << std::endl;
    return ins_camera::LogLevel::INFO;
}

// 镜头类型枚举 -> 可读名称(代号,非唯一标识,无隐私问题)。
inline const char* lensTypeName(ins_camera::CameraLensType t) {
    switch (t) {
    case ins_camera::CameraLensType::Wide577: return "Wide 577";
    case ins_camera::CameraLensType::Pano577: return "Pano 577";
    case ins_camera::CameraLensType::Wide283: return "Wide 283";
    case ins_camera::CameraLensType::Pano283: return "Pano 283";
    case ins_camera::CameraLensType::Wide586: return "Wide 586";
    case ins_camera::CameraLensType::Pano586: return "Pano 586";
    case ins_camera::CameraLensType::Action577: return "Action 577";
    case ins_camera::CameraLensType::PanoDefault: return "PanoDefault";
    default: return "Unknown";
    }
}

// Discover and open the first camera, registering default callbacks. Returns nullptr on failure.
// camera_type_out is optional: pass a non-null pointer to retrieve the model (for X6+ branch checks).
// log_path: resolved path from resolveLogPath(); empty means no log file.
// log_level: 日志级别，由命令行解析（见 parseLogLevel）。默认 ERR（仅错误）。
//   INFO 含下载采样点与 write_some 卡顿告警；VERBOSE 额外含每帧收发明细（刷屏量大）。
inline std::shared_ptr<ins_camera::Camera>
openCamera(ins_camera::LogLevel log_level = ins_camera::LogLevel::ERR,
           ins_camera::CameraType* camera_type_out = nullptr, const std::string& log_path = "") {
    if (!log_path.empty())
        ins_camera::SetLogPath(log_path);
    ins_camera::SetLogLevel(log_level);
    ins_camera::DeviceDiscovery discovery;
    auto list = discovery.GetAvailableDevices();
    if (list.empty()) {
        std::cerr << L("未发现设备", "no device found.") << std::endl;
        return nullptr;
    }
    if (camera_type_out)
        *camera_type_out = list[0].camera_type;
    const auto camera_type = list[0].camera_type; // 存一份:下方 FreeDeviceDescriptors 后 list 失效
    // 同样先存下要打印的基本信息(list 释放后失效);镜头需 Open() 后才可取,故统一放到打开成功后一行打印。
    const std::string serial = list[0].serial_number;
    const std::string camera_name = list[0].camera_name;
    const std::string fw_version = list[0].fw_version;
    auto cam = std::make_shared<ins_camera::Camera>(list[0].info);
    if (!cam->Open()) {
        std::cerr << L("打开相机失败", "failed to open camera") << std::endl;
        return nullptr;
    }
    discovery.FreeDeviceDescriptors(list);
    // SetStreamDelegate takes a non-const lvalue ref, so store it in a variable before passing it in.
    // The variable must be the base type shared_ptr<StreamDelegate>, otherwise a derived NullStreamDelegate instance
    // can't bind to the base lvalue ref.
    std::shared_ptr<ins_camera::StreamDelegate> stream_delegate = std::make_shared<NullStreamDelegate>();
    cam->SetStreamDelegate(stream_delegate);
    cam->SetCaptureStateNotification([](bool is_capture) {
        static bool last = false;
        if (is_capture == last) return;
        last = is_capture;
        std::cout << "\r[" << L("拍摄", "capture") << ": " << (is_capture ? L("ON", "ON") : L("OFF", "OFF"))
                  << "] " << std::flush;
        if (!is_capture) std::cout << std::endl;
    });
    cam->SetCaptureStoppedNotification([](const std::string& url, int err_code) {
        std::cout << "[" << L("拍摄已停止", "CAPTURE STOPPED") << "] err_code=" << err_code;
        if (!url.empty())
            std::cout << " url=" << url;
        std::cout << std::endl;
    });
    std::cout << L("打开相机成功", "Succeed to open camera.") << std::endl;
    // 一行打印基本信息;镜头规格仅 ONE R / ONE RS 可换镜头机型有意义,其他一体机不上报有效镜头
    // (默认 PANO_577),故不追加,避免误导。
    std::cout << L("序列号", "serial") << ":" << serial << "  " << L("机型", "type") << ":" << camera_name << "  "
              << L("固件", "fw") << ":" << fw_version << "  SDK:" << ins_camera::GetSDKVersion();
    if (camera_type == ins_camera::CameraType::Insta360OneR
        || camera_type == ins_camera::CameraType::Insta360OneRS) {
        auto lens = cam->GetCameraLensType();
        std::cout << "  " << L("镜头规格", "lens") << ":" << lensTypeName(lens) << " (" << static_cast<int>(lens)
                  << ")";
    }
    std::cout << std::endl;
    return cam;
}

// Generic picker: returns false on empty list / non-numeric input / out-of-range. On non-numeric input, clear+ignore
// fixes the cin infinite loop.
template <typename T, typename NameFn>
inline bool pickFromList(const std::vector<T>& list, const std::string& label, NameFn name_of, T& out) {
    if (list.empty()) {
        std::cerr << label << ": " << L("(列表为空)", "(empty)") << std::endl;
        return false;
    }
    std::cout << L("可选", "choose") << " " << label << ":" << std::endl;
    for (size_t i = 0; i < list.size(); ++i) {
        std::cout << "  [" << i << "] " << name_of(list[i]) << std::endl;
    }
    std::cout << L("请输入", "enter") << " " << label << " "
              << L("编号(输入 -1 取消返回上层)", "index (-1 to cancel): ");
    int idx = -1;
    if (!(std::cin >> idx)) {
        std::cin.clear();
        std::cin.ignore(0x7fffffff, '\n');
        std::cerr << L("输入无效:请输入数字编号", "invalid input: numeric index required") << std::endl;
        return false;
    }
    if (idx == -1) {
        std::cout << L("已取消,返回上层", "cancelled") << std::endl;
        return false;
    }
    if (idx < 0 || idx >= static_cast<int>(list.size())) {
        std::cerr << L("编号越界", "index out of range") << std::endl;
        return false;
    }
    out = list[idx];
    return true;
}

} // namespace demo_common
