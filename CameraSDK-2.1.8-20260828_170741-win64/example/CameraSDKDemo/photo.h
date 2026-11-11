// photo.h — photo sub-module for CameraSDKDemo
#pragma once
#include "../common/camera_setup.h"
#include "../common/file_utils.h"
#include "../common/i18n.h"
#include <iostream>
#include <set>
#include <string>
#include <vector>

extern std::shared_ptr<ins_camera::Camera> g_cam;
extern ins_camera::CameraType g_cameraType;

namespace photo {

using ins_camera::CameraFunctionMode;
using ins_camera::SubPhotoMode;

struct PhotoModeDesc {
    const char* name;
    const char* en_name;
    SubPhotoMode psub;
    CameraFunctionMode fmode;
    bool is_timelapse;
    bool is_hdr;
};
static const std::vector<PhotoModeDesc> kPhotoModes = {
    {"普通拍照", "single photo", SubPhotoMode::PHOTO_SINGLE, CameraFunctionMode::FUNCTION_MODE_NORMAL_IMAGE, false,
     false},
    {"HDR拍照", "HDR photo", SubPhotoMode::PHOTO_HDR, CameraFunctionMode::FUNCTION_MODE_HDR_IMAGE, false, true},
    {"间隔拍照", "interval photo", SubPhotoMode::PHOTO_INTERVAL, CameraFunctionMode::FUNCTION_MODE_INTERVAL_SHOOTING,
     true, false},
    {"连拍", "burst photo", SubPhotoMode::PHOTO_BURST, CameraFunctionMode::FUNCTION_MODE_BURST, false, false},
    {"星空拍照", "starlapse", SubPhotoMode::PHOTO_STARLAPSE, CameraFunctionMode::FUNCTION_MODE_STARLAPSE_IMAGE, true,
     false},
};
// Virtual HDR mode for cameras that use hdr_photo_mode attr on PHOTO_SINGLE (X4Air/X5/X6)
static const PhotoModeDesc kVirtualHdrMode = {
    "HDR拍照", "HDR photo", SubPhotoMode::PHOTO_SINGLE, CameraFunctionMode::FUNCTION_MODE_NORMAL_IMAGE, false, true};

struct PhotoConfig {
    const PhotoModeDesc* mode = nullptr;
    ins_camera::PhotoSize photo_size = ins_camera::PhotoSize::Size_11968_5984;
    std::string raw_type = "PURESHOT";
    int self_timer = 0;
    std::string lapse_time;
    std::string record_duration;
    std::string hdr_mode;
    int aeb_num = 3;
    double aeb_ev = 1.0;
    bool has_self_timer = false;
    bool has_lapse_time = false;
    bool has_duration = false;
    bool has_raw = false;
    bool has_hdr = false;
};
static PhotoConfig g_cfg;
bool g_capturing = false;
static bool g_initialized = false;

static void populateDefaults();

static void autoDetectMode() {
    if (g_initialized && g_cfg.mode)
        return;
    g_initialized = true;
    auto fmode = g_cam->GetCurrentFunctionMode();
    for (auto& m : kPhotoModes) {
        if (m.fmode == fmode) {
            g_cfg.mode = &m;
            populateDefaults();
            return;
        }
    }
}

static const char* photoSizeName(ins_camera::PhotoSize sz) {
    switch (sz) {
    case ins_camera::PhotoSize::Size_15520_7760: return "120MP (15520x7760)";
    case ins_camera::PhotoSize::Size_11968_5984: return "72MP (11968x5984)";
    case ins_camera::PhotoSize::Size_7776_3888: return "30MP (7776x3888)";
    case ins_camera::PhotoSize::Size_7680_3840: return "29MP (7680x3840)";
    case ins_camera::PhotoSize::Size_5952_2976: return "18MP (5952x2976)";
    case ins_camera::PhotoSize::Size_6912_3456: return "18MP (6912x3456)";
    default: return "";
    }
}

static void populateDefaults() {
    if (!g_cfg.mode)
        return;
    auto fmode = g_cfg.mode->fmode;

    auto sizes = g_cam->GetSupportedPhotoSizes(fmode);
    if (!sizes.empty())
        g_cfg.photo_size = sizes[0];

    auto rawVals = g_cam->GetSupportedAttrValues(fmode, "raw_capture_type");
    g_cfg.has_raw = !rawVals.empty();
    if (g_cfg.has_raw)
        g_cfg.raw_type = rawVals[0];

    auto timerVals = g_cam->GetSupportedAttrValues(fmode, "photography_self_timer");
    g_cfg.has_self_timer = !timerVals.empty();

    auto lapseVals = g_cam->GetSupportedAttrValues(fmode, "lapse_time");
    g_cfg.has_lapse_time = !lapseVals.empty();
    if (g_cfg.has_lapse_time)
        g_cfg.lapse_time = lapseVals[0];

    // 注意：拍照延时类（星空延时/间隔拍照）的 record_duration = “拍摄总时长/定时自停”，
    // 走延时 API（TimelapseParam.duration）。与循环录影(video.h)的 record_duration
    // = “单段循环片段长度”(走 RecordParams/PhotographyOptions.record_duration) 名同义不同。
    auto durVals = g_cam->GetSupportedAttrValues(fmode, "record_duration");
    g_cfg.has_duration = !durVals.empty();
    if (g_cfg.has_duration)
        g_cfg.record_duration = durVals[0];

    if (g_cfg.mode->is_hdr) {
        auto hdrVals = g_cam->GetSupportedAttrValues(fmode, "hdr_photo_mode");
        g_cfg.has_hdr = !hdrVals.empty();
        if (g_cfg.has_hdr)
            g_cfg.hdr_mode = hdrVals[0];
    }
}

static void printConfig() {
    if (!g_cfg.mode) {
        std::cout << demo_common::L("(未选模式)", "(no mode)") << std::endl;
        return;
    }
    std::cout << demo_common::L(g_cfg.mode->name, g_cfg.mode->en_name) << " | "
              << demo_common::L("默认尺寸", "Default size") << ": " << photoSizeName(g_cfg.photo_size);
    if (g_cfg.has_raw)
        std::cout << " | " << demo_common::L("格式", "fmt") << ": " << g_cfg.raw_type;
    if (g_cfg.has_self_timer)
        std::cout << " | " << demo_common::L("倒计时", "timer") << ": " << g_cfg.self_timer << "s";
    if (g_cfg.has_lapse_time)
        std::cout << " | " << demo_common::L("间隔", "interval") << ": " << g_cfg.lapse_time << "s";
    if (g_cfg.has_duration)
        std::cout << " | " << demo_common::L("时长", "duration") << ": "
                  << (g_cfg.record_duration == "0" ? "∞" : g_cfg.record_duration + "s");
    if (g_cfg.has_hdr)
        std::cout << " | " << demo_common::L("HDR", "HDR") << ": " << g_cfg.hdr_mode << " | AEB " << g_cfg.aeb_num
                  << "x" << g_cfg.aeb_ev;
    std::cout << std::endl;
}

// --- menu commands ---
static void cmdSelectMode() {
    auto supported = g_cam->GetSupportedPhotoModes();
    std::vector<const PhotoModeDesc*> cand;
    if (supported.empty()) {
        for (auto& m : kPhotoModes)
            cand.push_back(&m);
    } else {
        std::set<int> supported_set;
        for (auto fm : supported)
            supported_set.insert(static_cast<int>(fm));
        for (auto& m : kPhotoModes)
            if (supported_set.count(static_cast<int>(m.fmode)))
                cand.push_back(&m);
        // If PHOTO_SINGLE has hdr_photo_mode, add virtual HDR mode (X4Air/X5/X6)
        if (supported_set.count(static_cast<int>(CameraFunctionMode::FUNCTION_MODE_NORMAL_IMAGE))) {
            auto hdrVals =
                g_cam->GetSupportedAttrValues(CameraFunctionMode::FUNCTION_MODE_NORMAL_IMAGE, "hdr_photo_mode");
            if (!hdrVals.empty())
                cand.push_back(&kVirtualHdrMode);
        }
    }
    if (cand.empty()) {
        std::cout << demo_common::L("无可用拍照模式", "no photo modes") << std::endl;
        return;
    }
    std::cout << demo_common::L("可选拍照模式", "Available photo modes") << ":" << std::endl;
    for (size_t i = 0; i < cand.size(); ++i)
        std::cout << "  [" << i << "] " << demo_common::L(cand[i]->name, cand[i]->en_name) << std::endl;
    int idx = 0;
    std::cout << demo_common::L("选择", "choose") << ": ";
    if (!(std::cin >> idx) || idx < 0 || idx >= (int)cand.size()) {
        std::cin.clear();
        std::cin.ignore(0x7fffffff, '\n');
        return;
    }
    g_cfg = PhotoConfig{};
    g_cfg.mode = cand[idx];
    g_cam->SetPhotoSubMode(g_cfg.mode->psub);
    populateDefaults();
    // If virtual HDR mode, auto-set hdr_photo_mode to first available value
    if (cand[idx] == &kVirtualHdrMode) {
        auto hdrVals = g_cam->GetSupportedAttrValues(CameraFunctionMode::FUNCTION_MODE_NORMAL_IMAGE, "hdr_photo_mode");
        if (!hdrVals.empty()) {
            g_cfg.hdr_mode = hdrVals[0];
            g_cam->SetPhotoHdrMode(CameraFunctionMode::FUNCTION_MODE_NORMAL_IMAGE,
                                   static_cast<ins_camera::PhotoHdrType>(0));
            g_cfg.has_hdr = true;
        }
    }
    std::cout << demo_common::L("已选", "Selected") << ": " << demo_common::L(cand[idx]->name, cand[idx]->en_name)
              << std::endl;
    // auto fmode = g_cam->GetCurrentFunctionMode();
    // std::cout << demo_common::L("当前模式", "Current mode") << ": " << fmode << std::endl;
    printConfig();
}

static void cmdSetPhotoSize() {
    if (!g_cfg.mode) {
        std::cout << demo_common::L("请先选模式", "select mode first") << std::endl;
        return;
    }
    auto sizes = g_cam->GetSupportedPhotoSizes(g_cfg.mode->fmode);
    if (sizes.empty()) {
        std::cout << demo_common::L("无可用尺寸", "no sizes") << std::endl;
        return;
    }
    std::cout << demo_common::L("可选尺寸", "Available sizes") << ":" << std::endl;
    for (size_t i = 0; i < sizes.size(); ++i)
        std::cout << "  [" << i << "] " << photoSizeName(sizes[i]) << std::endl;
    int idx = 0;
    std::cout << demo_common::L("选择", "choose") << ": ";
    if (!(std::cin >> idx) || idx < 0 || idx >= (int)sizes.size()) {
        std::cin.clear();
        std::cin.ignore(0x7fffffff, '\n');
        return;
    }
    g_cfg.photo_size = sizes[idx];
    if (!g_cam->SetPhotoSize(g_cfg.mode->fmode, g_cfg.photo_size))
        std::cerr << demo_common::L("设置失败", "Set failed") << std::endl;
    else
        std::cout << demo_common::L("已选", "Selected") << ": " << photoSizeName(g_cfg.photo_size) << std::endl;
}

static void cmdSetRawType() {
    if (!g_cfg.mode || !g_cfg.has_raw) {
        std::cout << demo_common::L("不支持或未选模式", "not supported or no mode") << std::endl;
        return;
    }
    auto vals = g_cam->GetSupportedAttrValues(g_cfg.mode->fmode, "raw_capture_type");
    std::cout << demo_common::L("可选格式", "Available formats") << ":" << std::endl;
    for (size_t i = 0; i < vals.size(); ++i)
        std::cout << "  [" << i << "] " << vals[i] << std::endl;
    int idx = 0;
    std::cout << demo_common::L("选择", "choose") << ": ";
    if (!(std::cin >> idx) || idx < 0 || idx >= (int)vals.size()) {
        std::cin.clear();
        std::cin.ignore(0x7fffffff, '\n');
        return;
    }
    g_cfg.raw_type = vals[idx];
    int rawType = g_cam->GetAttrValueByName("raw_capture_type", vals[idx]);
    if (rawType >= 0)
        g_cam->SetRawCaptureType(g_cfg.mode->fmode, static_cast<ins_camera::RawCaptureType>(rawType));
    std::cout << demo_common::L("已设格式", "Format set") << ": " << vals[idx] << std::endl;
}

static void cmdSetSelfTimer() {
    if (!g_cfg.mode || !g_cfg.has_self_timer) {
        std::cout << demo_common::L("不支持", "not supported") << std::endl;
        return;
    }
    auto vals = g_cam->GetSupportedAttrValues(g_cfg.mode->fmode, "photography_self_timer");
    if (!vals.empty()) {
        std::cout << demo_common::L("可选", "Available") << ":";
        for (auto& v : vals)
            std::cout << " " << v;
        std::cout << std::endl;
    }
    std::cout << demo_common::L("倒计时秒数(0=关)", "timer seconds (0=off)") << ": ";
    if (!(std::cin >> g_cfg.self_timer)) {
        std::cin.clear();
        std::cin.ignore(0x7fffffff, '\n');
    }
    g_cam->SetPhotoSubModeTimer(g_cfg.mode->psub, g_cfg.self_timer);
    std::cout << demo_common::L("倒计时", "Timer") << ": " << g_cfg.self_timer << "s" << std::endl;
}

static void cmdSetLapseTime() {
    if (!g_cfg.mode || !g_cfg.has_lapse_time) {
        std::cout << demo_common::L("不支持", "not supported") << std::endl;
        return;
    }
    auto vals = g_cam->GetSupportedAttrValues(g_cfg.mode->fmode, "lapse_time");
    if (vals.empty()) {
        std::cout << demo_common::L("无数据", "no data") << std::endl;
        return;
    }
    std::cout << demo_common::L("可选间隔(秒)", "Available intervals") << ":" << std::endl;
    for (size_t i = 0; i < vals.size(); ++i)
        std::cout << "  [" << i << "] " << vals[i] << std::endl;
    int idx = 0;
    std::cout << demo_common::L("选择", "choose") << ": ";
    if (!(std::cin >> idx) || idx < 0 || idx >= (int)vals.size()) {
        std::cin.clear();
        std::cin.ignore(0x7fffffff, '\n');
        return;
    }
    g_cfg.lapse_time = vals[idx];
    std::cout << demo_common::L("间隔", "Interval") << ": " << g_cfg.lapse_time << "s" << std::endl;
}

static void cmdSetDuration() {
    if (!g_cfg.mode || !g_cfg.has_duration) {
        std::cout << demo_common::L("不支持", "not supported") << std::endl;
        return;
    }
    auto vals = g_cam->GetSupportedAttrValues(g_cfg.mode->fmode, "record_duration");
    if (vals.empty()) {
        std::cout << demo_common::L("无数据", "no data") << std::endl;
        return;
    }
    std::cout << demo_common::L("可选时长(秒)", "Available durations") << ":" << std::endl;
    for (size_t i = 0; i < vals.size(); ++i)
        std::cout << "  [" << i << "] " << (vals[i] == "0" ? "∞" : vals[i]) << std::endl;
    int idx = 0;
    std::cout << demo_common::L("选择", "choose") << ": ";
    if (!(std::cin >> idx) || idx < 0 || idx >= (int)vals.size()) {
        std::cin.clear();
        std::cin.ignore(0x7fffffff, '\n');
        return;
    }
    g_cfg.record_duration = vals[idx];
    std::cout << demo_common::L("时长", "Duration") << ": "
              << (g_cfg.record_duration == "0" ? "∞" : g_cfg.record_duration + "s") << std::endl;
}

static void cmdSetHdrMode() {
    if (!g_cfg.mode || !g_cfg.has_hdr) {
        std::cout << demo_common::L("仅HDR模式支持", "HDR mode only") << std::endl;
        return;
    }
    auto vals = g_cam->GetSupportedAttrValues(g_cfg.mode->fmode, "hdr_photo_mode");
    if (vals.empty()) {
        std::cout << demo_common::L("无数据", "no data") << std::endl;
        return;
    }
    std::cout << demo_common::L("可选HDR模式", "HDR modes") << ":" << std::endl;
    for (size_t i = 0; i < vals.size(); ++i)
        std::cout << "  [" << i << "] " << vals[i] << std::endl;
    int idx = 0;
    std::cout << demo_common::L("选择", "choose") << ": ";
    if (!(std::cin >> idx) || idx < 0 || idx >= (int)vals.size()) {
        std::cin.clear();
        std::cin.ignore(0x7fffffff, '\n');
        return;
    }
    g_cfg.hdr_mode = vals[idx];
    if (!g_cam->SetPhotoHdrMode(g_cfg.mode->fmode, static_cast<ins_camera::PhotoHdrType>(idx))) {
        std::cerr << demo_common::L("设置HDR模式失败", "Failed to set HDR mode") << std::endl;
        return;
    }
    if (g_cfg.hdr_mode == "AEB") {
        std::cout << demo_common::L("AEB张数", "AEB count") << " [" << g_cfg.aeb_num << "]: ";
        if (!(std::cin >> g_cfg.aeb_num)) {
            std::cin.clear();
            std::cin.ignore(0x7fffffff, '\n');
        }
        g_cam->SetAebCaptureNum(g_cfg.mode->fmode, g_cfg.aeb_num);
        std::cout << demo_common::L("AEB步长(EV)", "AEB step (EV)") << " [" << g_cfg.aeb_ev << "]: ";
        if (!(std::cin >> g_cfg.aeb_ev)) {
            std::cin.clear();
            std::cin.ignore(0x7fffffff, '\n');
        }
        g_cam->SetAebExposureBias(g_cfg.mode->fmode, g_cfg.aeb_ev);
    }
    std::cout << demo_common::L("HDR已设", "HDR set") << ": " << g_cfg.hdr_mode << std::endl;
}

static void cmdStartCapture() {
    if (!g_cfg.mode) {
        std::cout << demo_common::L("请先选模式", "select mode first") << std::endl;
        return;
    }
    // Check SD card before capture
    ins_camera::StorageStatus st;
    if (g_cam->GetStorageState(st)) {
        if (st.state == ins_camera::STOR_CS_NOCARD) {
            std::cerr << demo_common::L("无SD卡，无法拍摄", "No SD card, cannot capture") << std::endl;
            return;
        }
        if (st.state == ins_camera::STOR_CS_NOSPACE) {
            std::cerr << demo_common::L("SD卡已满，无法拍摄", "SD card full, cannot capture") << std::endl;
            return;
        }
    }
    if (!g_cam->SetPhotoSize(g_cfg.mode->fmode, g_cfg.photo_size)) {
        std::cerr << demo_common::L("设置照片尺寸失败", "Failed to set photo size") << std::endl;
        return;
    }
    if (g_cfg.has_raw) {
        int rawType = g_cam->GetAttrValueByName("raw_capture_type", g_cfg.raw_type);
        if (rawType >= 0)
            g_cam->SetRawCaptureType(g_cfg.mode->fmode, static_cast<ins_camera::RawCaptureType>(rawType));
    }

    if (g_cfg.mode->is_timelapse) {
        g_capturing = true;
        auto tmode = ins_camera::CameraTimelapseMode::MOBILE_TIMELAPSE_VIDEO;
        if (g_cfg.mode->fmode == CameraFunctionMode::FUNCTION_MODE_INTERVAL_SHOOTING)
            tmode = ins_camera::CameraTimelapseMode::TIMELAPSE_INTERVAL_SHOOTING;
        else if (g_cfg.mode->fmode == CameraFunctionMode::FUNCTION_MODE_STARLAPSE_IMAGE)
            tmode = ins_camera::CameraTimelapseMode::TIMELAPSE_STARLAPSE_SHOOTING;

        ins_camera::TimelapseParam tp;
        tp.mode = tmode;
        if (!g_cfg.lapse_time.empty()) {
            uint32_t lt = static_cast<uint32_t>(std::atof(g_cfg.lapse_time.c_str()) * 1000.0);
            tp.lapseTime = lt < 500 ? 500 : lt;
        }
        if (!g_cfg.record_duration.empty())
            tp.duration = static_cast<uint32_t>(std::atof(g_cfg.record_duration.c_str())); // 拍摄总时长，定时自停
        if (!g_cam->SetTimeLapseOption(tp)) {
            std::cerr << demo_common::L("设置延时参数失败", "Failed to set timelapse options") << std::endl;
            g_capturing = false;
            return;
        }

        bool ok = g_cam->StartTimeLapse(tmode);
        std::cout << (ok ? demo_common::L("已开始(选9停止)", "Started (9 to stop)")
                         : demo_common::L("启动失败", "Start failed"))
                  << std::endl;
        if (!ok)
            g_capturing = false;
    } else if (g_cfg.mode->is_hdr && g_cfg.hdr_mode == "AEB") {
        bool is_aeb = (g_cfg.hdr_mode == "AEB");
        auto urls =
            g_cam->StartHDRCapture(g_cfg.photo_size, is_aeb, g_cfg.aeb_num, static_cast<int>(g_cfg.aeb_ev * 10));
        if (!urls.Empty())
            std::cout << demo_common::L("HDR(AEB)已拍摄", "HDR(AEB) taken") << ": " << urls.GetSingleOrigin()
                      << std::endl;
        else
            std::cerr << demo_common::L("HDR(AEB)拍摄失败", "HDR(AEB) failed") << std::endl;
    } else if (g_cfg.mode->is_hdr) {
        auto urls = g_cam->StartHDRCapture(g_cfg.photo_size);
        if (!urls.Empty())
            std::cout << demo_common::L("HDR已拍摄", "HDR taken") << ": " << urls.GetSingleOrigin() << std::endl;
        else
            std::cerr << demo_common::L("HDR拍摄失败", "HDR failed") << std::endl;
    } else {
        auto url = g_cam->TakePhoto();
        if (!url.Empty())
            std::cout << demo_common::L("拍照成功", "OK") << ": " << url.GetSingleOrigin() << std::endl;
        else
            std::cerr << demo_common::L("拍照失败", "Failed") << std::endl;
    }
}

static void cmdStopCapture() {
    if (!g_cam->CaptureCurrentStatus()) {
        std::cout << demo_common::L("未在拍摄", "not capturing") << std::endl;
        return;
    }
    auto tmode = ins_camera::CameraTimelapseMode::MOBILE_TIMELAPSE_VIDEO;
    if (g_cfg.mode && g_cfg.mode->fmode == CameraFunctionMode::FUNCTION_MODE_INTERVAL_SHOOTING)
        tmode = ins_camera::CameraTimelapseMode::TIMELAPSE_INTERVAL_SHOOTING;
    else if (g_cfg.mode && g_cfg.mode->fmode == CameraFunctionMode::FUNCTION_MODE_STARLAPSE_IMAGE)
        tmode = ins_camera::CameraTimelapseMode::TIMELAPSE_STARLAPSE_SHOOTING;
    auto url = g_cam->StopTimeLapse(tmode);
    g_capturing = false;
    if (!url.Empty())
        std::cout << demo_common::L("已停止", "Stopped") << ": " << url.GetSingleOrigin() << std::endl;
    else
        std::cout << demo_common::L("已停止(可能任务已结束)", "Stopped (task may have ended)") << std::endl;
}

static void cmdTakePhotoAndDownload() {
    if (!g_cfg.mode) {
        std::cout << demo_common::L("请先选模式", "select mode first") << std::endl;
        return;
    }
    // Check SD card before capture
    ins_camera::StorageStatus st;
    if (g_cam->GetStorageState(st) && st.state == ins_camera::STOR_CS_NOCARD) {
        std::cerr << demo_common::L("无SD卡，无法拍摄", "No SD card, cannot capture") << std::endl;
        return;
    }
    auto url = g_cam->TakePhoto();
    if (url.Empty()) {
        std::cerr << demo_common::L("拍照失败", "Failed") << std::endl;
        return;
    }
    std::cout << demo_common::L("拍照成功", "OK") << ": " << url.GetSingleOrigin() << std::endl;
    std::string dir;
    std::cout << demo_common::L("保存目录", "save directory") << ": ";
    std::cin >> dir;
    std::replace(dir.begin(), dir.end(), '\\', '/');
    if (!demo_common::fileIsExist(dir)) {
        std::cout << demo_common::L("目录不存在", "dir not found") << std::endl;
        return;
    }
    if (!demo_common::endsWithSlash(dir))
        dir += "/";
    std::string remote = url.GetSingleOrigin();
    std::string fname = demo_common::GetFileName(remote);
    demo_common::downloadFileFromCamera(g_cam, remote, dir + fname);
}

static void cmdBrowseAttrs() {
    if (!g_cfg.mode) {
        std::cout << demo_common::L("请先选模式", "select mode first") << std::endl;
        return;
    }
    auto attrs = g_cam->GetSupportedAttrNames(g_cfg.mode->fmode);
    if (attrs.empty()) {
        std::cout << demo_common::L("无属性", "no attrs") << std::endl;
        return;
    }
    std::cout << attrs.size() << " " << demo_common::L("个属性", "attrs") << ":" << std::endl;
    for (auto& a : attrs) {
        std::cout << "  " << a;
        auto deps = g_cam->GetAttrDependOn(g_cfg.mode->fmode, a);
        if (!deps.empty()) {
            std::cout << " (";
            for (size_t i = 0; i < deps.size(); ++i) {
                if (i)
                    std::cout << ", ";
                std::cout << deps[i];
            }
            std::cout << ")";
        }
        auto vals = g_cam->GetSupportedAttrValues(g_cfg.mode->fmode, a);
        if (!vals.empty()) {
            std::cout << " [";
            int n = (std::min)((int)vals.size(), 6);
            for (int i = 0; i < n; ++i) {
                if (i)
                    std::cout << " ";
                std::cout << vals[i];
            }
            if ((int)vals.size() > 6)
                std::cout << " ...";
            std::cout << "]";
        }
        std::cout << std::endl;
    }
}

// --- main menu ---
inline void run() {
    // 诊断:进入拍照前打印当前模式与能力表可用性,便于定位"进拍照即退出"类问题
    // (如能力表为空时各查询返回空,便于区分是数据缺失还是逻辑问题)。
    {
        auto fmode = g_cam->GetCurrentFunctionMode();
        auto attrs = g_cam->GetSupportedAttrNames(fmode);
        std::cout << demo_common::L("[诊断] 当前功能模式=", "[diag] function mode=") << static_cast<int>(fmode)
                  << demo_common::L(" 能力表属性数=", " capability attr count=") << attrs.size() << std::endl;
        if (attrs.empty())
            std::cout << demo_common::L("[诊断] 能力表为空: 可能未匹配到 JSON 或加载失败,拍照配置将取默认值",
                                        "[diag] capability empty: JSON not matched or load failed; using defaults")
                      << std::endl;
    }
    autoDetectMode();
    while (true) {
        std::cout << "\n=== " << demo_common::L("拍照", "Photo") << " ===\n";
        printConfig();
        std::cout << "  1: " << demo_common::L("选择模式", "select mode") << "\n"
                  << "  2: " << demo_common::L("设照片尺寸", "set photo size") << "\n";
        if (g_cfg.has_raw)
            std::cout << "  3: " << demo_common::L("设照片格式", "set photo format") << "\n";
        if (g_cfg.has_self_timer)
            std::cout << "  4: " << demo_common::L("设倒计时", "set self-timer") << "\n";
        if (g_cfg.has_lapse_time)
            std::cout << "  5: " << demo_common::L("设间隔", "set interval") << "\n";
        if (g_cfg.has_duration)
            std::cout << "  6: " << demo_common::L("设时长", "set duration") << "\n";
        if (g_cfg.has_hdr)
            std::cout << "  7: " << demo_common::L("HDR参数", "HDR settings") << "\n";
        std::cout << "  8: " << demo_common::L("开始拍摄", "start capture") << "\n"
                  << "  9: " << demo_common::L("停止(间隔/星空)", "stop (interval/starlapse)") << "\n"
                  << " 10: " << demo_common::L("拍照并下载", "take photo + download") << "\n"
                  << " 11: " << demo_common::L("浏览属性", "browse attrs") << "\n"
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
        case 1: cmdSelectMode(); break;
        case 2: cmdSetPhotoSize(); break;
        case 3: cmdSetRawType(); break;
        case 4: cmdSetSelfTimer(); break;
        case 5: cmdSetLapseTime(); break;
        case 6: cmdSetDuration(); break;
        case 7: cmdSetHdrMode(); break;
        case 8: cmdStartCapture(); break;
        case 9: cmdStopCapture(); break;
        case 10: cmdTakePhotoAndDownload(); break;
        case 11: cmdBrowseAttrs(); break;
        default: std::cout << demo_common::L("无效", "invalid") << std::endl; break;
        }
    }
}

} // namespace photo