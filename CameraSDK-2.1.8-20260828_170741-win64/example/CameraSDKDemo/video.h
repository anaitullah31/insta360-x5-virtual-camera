// video.h — video recording sub-module for CameraSDKDemo
#pragma once
#include "../common/camera_setup.h"
#include "../common/i18n.h"
#include <camera/photography_settings.h>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

extern std::shared_ptr<ins_camera::Camera> g_cam;
extern ins_camera::CameraType g_cameraType;

namespace video {

using ins_camera::CameraFunctionMode;
using ins_camera::CameraTimelapseMode;
using ins_camera::SubVideoMode;
using ins_camera::VideoResolution;

// --- mode descriptors ---
struct VideoModeDesc {
    const char* name;
    const char* en_name;
    SubVideoMode vsub;
    CameraFunctionMode fmode;
};
static const std::vector<VideoModeDesc> kVideoModes = {
    {"普通录像", "normal video", SubVideoMode::VIDEO_NORMAL, CameraFunctionMode::FUNCTION_MODE_NORMAL_VIDEO},
    {"夜景录像", "night video", SubVideoMode::VIDEO_PURE, CameraFunctionMode::FUNCTION_MODE_PURE_VIDEO},
    {"HDR视频", "HDR video", SubVideoMode::VIDEO_HDR, CameraFunctionMode::FUNCTION_MODE_HDR_VIDEO},
    {"延时摄影", "timelapse", SubVideoMode::VIDEO_TIMELAPSE, CameraFunctionMode::FUNCTION_MODE_STATIC_TIMELAPSE},
    {"移动延时", "timeshift", SubVideoMode::VIDEO_TIMESHIFT, CameraFunctionMode::FUNCTION_MODE_TIMESHIFT},
    {"子弹时间", "bullet time", SubVideoMode::VIDEO_BULLETTIME, CameraFunctionMode::FUNCTION_MODE_BULLETTIME},
    {"循环录影", "loop recording", SubVideoMode::VIDEO_LOOPRECORDING,
     CameraFunctionMode::FUNCTION_MODE_LOOP_RECORDING_VIDEO},
    {"行车记录仪", "dashcam", SubVideoMode::VIDEO_DASH_CAM, CameraFunctionMode::FUNCTION_MODE_DASH_CAM},
};

// --- config ---
struct VideoConfig {
    const VideoModeDesc* mode = nullptr;
    VideoResolution resolution = VideoResolution::RES_3840_1920P30;
    std::string res_name;
    std::string scene;
    std::string lapse_time;
    std::string record_duration;
    int accelerate = 0;
    bool is_timelapse = false;
    bool has_accel = false;
    bool has_duration = false; // 模式是否支持 record_duration（如循环录影、延时）
};
static VideoConfig g_cfg;
bool g_running = false;
static bool g_is_timelapse = false;
static bool g_initialized = false;
static CameraTimelapseMode g_last_tmode = CameraTimelapseMode::TIMELAPSE_MIXED;

static void populateDefaults();

static void autoDetectMode() {
    if (g_initialized && g_cfg.mode)
        return;
    g_initialized = true;
    auto fmode = g_cam->GetCurrentFunctionMode();
    for (auto& m : kVideoModes) {
        if (m.fmode == fmode) {
            g_cfg.mode = &m;
            populateDefaults();
            return;
        }
    }
}

static bool modeHasAttr(CameraFunctionMode fmode, const std::string& attr) {
    auto vals = g_cam->GetSupportedAttrValues(fmode, attr);
    return !vals.empty();
}

static std::string resLabel(int w, int h) {
    if (w == 7680 && h == 3840)
        return "8K (7680x3840)";
    if (w == 6016 && h == 3008)
        return "6K (6016x3008)";
    if (w == 3840 && h == 1920)
        return "4K (3840x1920)";
    if (w == 3840 && h == 3840)
        return "8K (3840x3840)";
    if (w == 3008 && h == 3008)
        return "6K (3008x3008)";
    if (w == 5760 && h == 1440)
        return "5.7K (5760x1440)";
    if (w == 7680 && h == 1920)
        return "BulletTime (7680x1920)";
    if (w == 6016 && h == 1504)
        return "BulletTime (6016x1504)";
    if (w == 3840 && h == 960)
        return "BulletTime (3840x960)";
    return std::to_string(w) + "x" + std::to_string(h);
}

static void populateDefaults() {
    if (!g_cfg.mode)
        return;
    auto fmode = g_cfg.mode->fmode;
    g_cfg.is_timelapse = modeHasAttr(fmode, "lapse_time");
    g_cfg.has_accel = !g_cam->GetSupportedAccelerateFrequencies(fmode).empty();
    // 录像里 record_duration 仅循环录影(VIDEO_LOOPRECORDING)支持，含义 = “单段循环片段长度”
    // (滚动覆盖窗口，录制本身不停)，走 RecordParams.record_duration → PhotographyOptions.record_duration。
    // 与延时摄影只有 lapse_time(间隔)、以及拍照延时类的 record_duration(总时长/定时自停) 均不同。
    g_cfg.has_duration = modeHasAttr(fmode, "record_duration");

    auto resolutions = g_cam->GetSupportedVideoResolutions(fmode);
    if (!resolutions.empty()) {
        g_cfg.resolution = resolutions[0];
        g_cfg.res_name = g_cam->GetVideoResolutionName(resolutions[0]);
    }
    if (g_cfg.is_timelapse) {
        auto scenes = g_cam->GetSupportedAttrValues(fmode, "timelapse_scene");
        g_cfg.scene = scenes.empty() ? "" : scenes[0];
        auto lv = g_cam->GetSupportedAttrValues(fmode, "lapse_time", g_cfg.res_name + "|" + g_cfg.scene);
        if (lv.empty())
            lv = g_cam->GetSupportedAttrValues(fmode, "lapse_time");
        g_cfg.lapse_time = lv.empty() ? "" : lv[0];
    }
    // 时长默认值：延时按 场景|间隔 依赖查询，其它模式（如循环录影）直接查。
    if (g_cfg.has_duration) {
        auto dv = g_cfg.is_timelapse
                      ? g_cam->GetSupportedAttrValues(fmode, "record_duration", g_cfg.scene + "|" + g_cfg.lapse_time)
                      : std::vector<std::string>{};
        if (dv.empty())
            dv = g_cam->GetSupportedAttrValues(fmode, "record_duration");
        g_cfg.record_duration = dv.empty() ? "0" : dv[0];
    } else {
        g_cfg.record_duration = "0";
    }
    if (g_cfg.has_accel) {
        auto accels = g_cam->GetSupportedAccelerateFrequencies(fmode);
        g_cfg.accelerate = accels.empty() ? 0 : accels[0];
    }
}

static void printConfig() {
    if (!g_cfg.mode) {
        std::cout << demo_common::L("(未选模式)", "(no mode)") << std::endl;
        return;
    }
    std::cout << demo_common::L("模式", "Mode") << ": " << demo_common::L(g_cfg.mode->name, g_cfg.mode->en_name)
              << " | " << demo_common::L("默认分辨率", "Default res") << ": " << g_cfg.res_name;
    if (g_cfg.is_timelapse) {
        std::cout << " | " << demo_common::L("场景", "scene") << ": " << g_cfg.scene << " | "
                  << demo_common::L("间隔", "interval") << ": " << g_cfg.lapse_time << "s";
    }
    if (g_cfg.has_duration) {
        std::cout << " | " << demo_common::L("时长", "duration") << ": "
                  << (g_cfg.record_duration == "0" ? "∞" : g_cfg.record_duration + "s");
    }
    if (g_cfg.has_accel)
        std::cout << " | " << demo_common::L("倍速", "speed") << ": " << g_cfg.accelerate;
    std::cout << std::endl;
}

// --- menu commands ---
static void cmdSelectMode() {
    // Query capability table for supported modes; fall back to static list if unavailable.
    auto supported = g_cam->GetSupportedVideoModes();
    std::vector<const VideoModeDesc*> cand;
    if (supported.empty()) {
        for (auto& m : kVideoModes)
            cand.push_back(&m);
    } else {
        std::set<int> supported_set;
        for (auto fm : supported)
            supported_set.insert(static_cast<int>(fm));
        for (auto& m : kVideoModes)
            if (supported_set.count(static_cast<int>(m.fmode)))
                cand.push_back(&m);
    }
    if (cand.empty()) {
        std::cout << demo_common::L("无可用录像模式", "no video modes") << std::endl;
        return;
    }
    std::cout << demo_common::L("可选录像模式", "Available video modes") << ":" << std::endl;
    for (size_t i = 0; i < cand.size(); ++i)
        std::cout << "  [" << i << "] " << demo_common::L(cand[i]->name, cand[i]->en_name) << std::endl;
    int idx = 0;
    std::cout << demo_common::L("选择", "choose") << ": ";
    if (!(std::cin >> idx) || idx < 0 || idx >= (int)cand.size()) {
        std::cin.clear();
        std::cin.ignore(0x7fffffff, '\n');
        return;
    }
    g_cfg = VideoConfig{};
    g_cfg.mode = cand[idx];
    g_cam->SetVideoSubMode(g_cfg.mode->vsub);
    populateDefaults();
    std::cout << demo_common::L("已选", "Selected") << ": " << demo_common::L(cand[idx]->name, cand[idx]->en_name)
              << std::endl;
    printConfig();
}

static void cmdSetResolution() {
    if (!g_cfg.mode) {
        std::cout << demo_common::L("请先选模式", "select mode first") << std::endl;
        return;
    }
    auto resolutions = g_cam->GetSupportedVideoResolutions(g_cfg.mode->fmode);
    if (resolutions.empty()) {
        std::cout << demo_common::L("无可用分辨率", "no resolutions") << std::endl;
        return;
    }

    // Build tiered list: group by resolution, show FPS options under each
    std::map<std::string, std::vector<std::pair<std::string, VideoResolution>>> tierMap;
    for (auto res : resolutions) {
        std::string name = g_cam->GetVideoResolutionName(res);
        if (name.empty())
            continue;
        auto p1 = name.find('_');
        auto p2 = (p1 == std::string::npos) ? std::string::npos : name.find('_', p1 + 1);
        if (p1 == std::string::npos || p2 == std::string::npos)
            continue;
        int w = std::atoi(name.substr(0, p1).c_str());
        int h = std::atoi(name.substr(p1 + 1, p2 - p1 - 1).c_str());
        if (w <= 0 || h <= 0)
            continue;
        std::string fps = name.substr(p2 + 1);
        std::string key = resLabel(w, h);
        tierMap[key].push_back({fps, res});
    }

    // Flatten to indexed list
    std::vector<std::pair<std::string, VideoResolution>> flat;
    std::cout << demo_common::L("可选分辨率", "Available resolutions") << ":" << std::endl;
    for (auto& kv : tierMap) {
        std::cout << "  " << kv.first << ":" << std::endl;
        for (auto& fp : kv.second) {
            int idx = (int)flat.size();
            std::cout << "    [" << idx << "] " << fp.first << "fps" << std::endl;
            flat.push_back(fp);
        }
    }
    std::cout << demo_common::L("选择(-1跳过)", "choose (-1 skip)") << ": ";
    int idx = -1;
    if (!(std::cin >> idx)) {
        std::cin.clear();
        std::cin.ignore(0x7fffffff, '\n');
        return;
    }
    if (idx >= 0 && idx < (int)flat.size()) {
        g_cfg.resolution = flat[idx].second;
        g_cfg.res_name = g_cam->GetVideoResolutionName(flat[idx].second);
        if (g_cfg.is_timelapse) {
            auto lv =
                g_cam->GetSupportedAttrValues(g_cfg.mode->fmode, "lapse_time", g_cfg.res_name + "|" + g_cfg.scene);
            if (lv.empty())
                lv = g_cam->GetSupportedAttrValues(g_cfg.mode->fmode, "lapse_time");
            g_cfg.lapse_time = lv.empty() ? "" : lv[0];
        }
        std::cout << demo_common::L("已选", "Selected") << ": " << g_cfg.res_name << std::endl;
    }
}

static void cmdSetScene() {
    if (!g_cfg.mode || !g_cfg.is_timelapse) {
        std::cout << demo_common::L("仅延时摄影支持", "timelapse only") << std::endl;
        return;
    }
    auto scenes = g_cam->GetSupportedAttrValues(g_cfg.mode->fmode, "timelapse_scene");
    if (scenes.empty()) {
        std::cout << demo_common::L("无场景", "no scenes") << std::endl;
        return;
    }
    std::cout << demo_common::L("可选场景", "Available scenes") << ":" << std::endl;
    for (size_t i = 0; i < scenes.size(); ++i)
        std::cout << "  [" << i << "] " << scenes[i] << std::endl;
    int idx = 0;
    std::cout << demo_common::L("选择", "choose") << ": ";
    if (!(std::cin >> idx) || idx < 0 || idx >= (int)scenes.size()) {
        std::cin.clear();
        std::cin.ignore(0x7fffffff, '\n');
        return;
    }
    g_cfg.scene = scenes[idx];
    auto lv = g_cam->GetSupportedAttrValues(g_cfg.mode->fmode, "lapse_time", g_cfg.res_name + "|" + g_cfg.scene);
    if (lv.empty())
        lv = g_cam->GetSupportedAttrValues(g_cfg.mode->fmode, "lapse_time");
    g_cfg.lapse_time = lv.empty() ? "" : lv[0];
    auto dv = g_cam->GetSupportedAttrValues(g_cfg.mode->fmode, "record_duration", g_cfg.scene + "|" + g_cfg.lapse_time);
    if (dv.empty())
        dv = g_cam->GetSupportedAttrValues(g_cfg.mode->fmode, "record_duration");
    g_cfg.record_duration = dv.empty() ? "0" : dv[0];
    std::cout << demo_common::L("场景", "Scene") << ": " << g_cfg.scene << " | " << demo_common::L("间隔", "interval")
              << ": " << g_cfg.lapse_time << "s | " << demo_common::L("时长", "duration") << ": "
              << (g_cfg.record_duration == "0" ? "∞" : g_cfg.record_duration + "s") << std::endl;
}

static void cmdSetLapseTime() {
    if (!g_cfg.mode || !g_cfg.is_timelapse) {
        std::cout << demo_common::L("仅延时摄影支持", "timelapse only") << std::endl;
        return;
    }
    auto vals = g_cam->GetSupportedAttrValues(g_cfg.mode->fmode, "lapse_time", g_cfg.res_name + "|" + g_cfg.scene);
    if (vals.empty())
        vals = g_cam->GetSupportedAttrValues(g_cfg.mode->fmode, "lapse_time");
    if (vals.empty()) {
        std::cout << demo_common::L("无数据", "no data") << std::endl;
        return;
    }
    std::cout << demo_common::L("可选间隔(秒)", "Available intervals (sec)") << ":" << std::endl;
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
    auto dv = g_cam->GetSupportedAttrValues(g_cfg.mode->fmode, "record_duration", g_cfg.scene + "|" + g_cfg.lapse_time);
    if (dv.empty())
        dv = g_cam->GetSupportedAttrValues(g_cfg.mode->fmode, "record_duration");
    g_cfg.record_duration = dv.empty() ? "0" : dv[0];
    std::cout << demo_common::L("间隔", "Interval") << ": " << g_cfg.lapse_time << "s | "
              << demo_common::L("时长", "duration") << ": "
              << (g_cfg.record_duration == "0" ? "∞" : g_cfg.record_duration + "s") << std::endl;
}

static void cmdSetDuration() {
    if (!g_cfg.mode || !g_cfg.has_duration) {
        std::cout << demo_common::L("该模式不支持时长设置", "duration not supported") << std::endl;
        return;
    }
    // 延时按 场景|间隔 依赖查询；其它模式（如循环录影）直接查该属性全部可选值。
    auto vals = g_cfg.is_timelapse ? g_cam->GetSupportedAttrValues(g_cfg.mode->fmode, "record_duration",
                                                                   g_cfg.scene + "|" + g_cfg.lapse_time)
                                   : std::vector<std::string>{};
    if (vals.empty())
        vals = g_cam->GetSupportedAttrValues(g_cfg.mode->fmode, "record_duration");
    if (vals.empty()) {
        std::cout << demo_common::L("无数据", "no data") << std::endl;
        return;
    }
    std::cout << demo_common::L("可选时长(秒)", "Available durations (sec)") << ":" << std::endl;
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

static void cmdSetAccelerate() {
    if (!g_cfg.mode || !g_cfg.has_accel) {
        std::cout << demo_common::L("不支持加速", "accel not supported") << std::endl;
        return;
    }
    auto freqs = g_cam->GetSupportedAccelerateFrequencies(g_cfg.mode->fmode);
    if (freqs.empty()) {
        std::cout << demo_common::L("无数据", "no data") << std::endl;
        return;
    }
    std::cout << demo_common::L("可选倍速", "Available speeds") << ":" << std::endl;
    for (size_t i = 0; i < freqs.size(); ++i)
        std::cout << "  [" << i << "] " << (freqs[i] == 0 ? demo_common::L("自动", "Auto") : std::to_string(freqs[i]))
                  << std::endl;
    int idx = 0;
    std::cout << demo_common::L("选择", "choose") << ": ";
    if (!(std::cin >> idx) || idx < 0 || idx >= (int)freqs.size()) {
        std::cin.clear();
        std::cin.ignore(0x7fffffff, '\n');
        return;
    }
    g_cfg.accelerate = freqs[idx];
    std::cout << demo_common::L("倍速", "Speed") << ": " << g_cfg.accelerate << std::endl;
}

static void cmdStartCapture() {
    if (!g_cfg.mode) {
        std::cout << demo_common::L("请先选模式", "select mode first") << std::endl;
        return;
    }
    if (g_cam->CaptureCurrentStatus()) {
        std::cout << demo_common::L("已在拍摄中", "already capturing") << std::endl;
        return;
    }
    // Check SD card
    ins_camera::StorageStatus st;
    if (g_cam->GetStorageState(st) && st.state == ins_camera::STOR_CS_NOCARD) {
        std::cerr << demo_common::L("无SD卡，无法拍摄", "No SD card, cannot capture") << std::endl;
        return;
    }
    ins_camera::RecordParams rp;
    rp.resolution = g_cfg.resolution;
    rp.accelerate_fequency = g_cfg.accelerate;
    // 非延时模式（如循环录影）的时长通过 RecordParams 下发；延时走 SetTimeLapseOption。
    if (!g_cfg.is_timelapse && g_cfg.has_duration && !g_cfg.record_duration.empty())
        rp.record_duration = static_cast<uint32_t>(std::atof(g_cfg.record_duration.c_str()));
    if (!g_cam->SetVideoCaptureParams(rp, g_cfg.mode->fmode)) {
        std::cerr << demo_common::L("设置录像参数失败", "Failed to set video params") << std::endl;
        return;
    }

    if (g_cfg.is_timelapse) {
        ins_camera::TimelapseParam tp;
        tp.mode = ins_camera::CameraTimelapseMode::MOBILE_TIMELAPSE_VIDEO;
        // lapseTime is in ms; capability table values are in seconds (may be float like "0.5")
        tp.duration =
            g_cfg.record_duration.empty() ? 0 : static_cast<uint32_t>(std::atof(g_cfg.record_duration.c_str()));
        uint32_t lt = static_cast<uint32_t>(std::atof(g_cfg.lapse_time.c_str()) * 1000.0);
        tp.lapseTime = g_cfg.lapse_time.empty() ? 1000 : (lt < 500 ? 500 : lt);
        tp.accelerate_fequency = g_cfg.accelerate;
        if (!g_cam->SetTimeLapseOption(tp)) {
            std::cerr << demo_common::L("设置延时参数失败", "Failed to set timelapse options") << std::endl;
            return;
        }
        if (g_cam->StartTimeLapse(tp.mode)) {
            g_running = true;
            g_is_timelapse = true;
            g_last_tmode = tp.mode;
            std::cout << demo_common::L("延时已开始", "Timelapse started") << std::endl;
        } else
            std::cerr << demo_common::L("启动延时失败", "Failed to start timelapse") << std::endl;
    } else {
        if (g_cam->StartRecording()) {
            g_running = true;
            g_is_timelapse = false;
            std::cout << demo_common::L("录像已开始", "Recording started") << std::endl;
        } else
            std::cout << demo_common::L("失败", "Failed") << std::endl;
    }
}

static void cmdStopCapture() {
    if (!g_cam->CaptureCurrentStatus()) {
        std::cout << demo_common::L("未在拍摄", "not capturing") << std::endl;
        return;
    }
    if (g_is_timelapse) {
        auto url = g_cam->StopTimeLapse(g_last_tmode);
        if (!url.Empty())
            std::cout << demo_common::L("延时已停止", "Timelapse stopped") << ": " << url.GetSingleOrigin()
                      << std::endl;
    } else {
        auto url = g_cam->StopRecording();
        if (!url.Empty()) {
            std::cout << demo_common::L("录像已停止", "Recording stopped") << ": ";
            for (auto& u : url.OriginUrls())
                std::cout << u << " ";
            std::cout << std::endl;
        }
    }
    g_running = false;
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
    autoDetectMode();
    while (true) {
        std::cout << "\n=== " << demo_common::L("录像", "Video") << " ===\n";
        printConfig();
        std::cout << "  1: " << demo_common::L("选择模式", "select mode") << "\n"
                  << "  2: " << demo_common::L("设置分辨率", "set resolution") << "\n";
        if (g_cfg.is_timelapse) {
            std::cout << "  3: " << demo_common::L("设场景", "set scene") << "\n"
                      << "  4: " << demo_common::L("设间隔", "set interval") << "\n";
        }
        if (g_cfg.has_duration)
            std::cout << "  5: " << demo_common::L("设时长", "set duration") << "\n";
        if (g_cfg.has_accel)
            std::cout << "  6: " << demo_common::L("设加速倍数", "set accelerate") << "\n";
        std::cout << "  7: " << demo_common::L("开始拍摄", "start capture") << "\n"
                  << "  8: " << demo_common::L("停止拍摄", "stop capture") << "\n"
                  << "  9: " << demo_common::L("浏览属性", "browse attrs") << "\n"
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
        case 2: cmdSetResolution(); break;
        case 3: cmdSetScene(); break;
        case 4: cmdSetLapseTime(); break;
        case 5: cmdSetDuration(); break;
        case 6: cmdSetAccelerate(); break;
        case 7: cmdStartCapture(); break;
        case 8: cmdStopCapture(); break;
        case 9: cmdBrowseAttrs(); break;
        default: std::cout << demo_common::L("无效", "invalid") << std::endl; break;
        }
    }
}

} // namespace video