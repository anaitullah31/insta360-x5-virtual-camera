// capture_settings.h — exposure & image settings sub-module for CameraSDKDemo
//
// SDK API calls are marked with "=== SDK API ===" comments.
// Everything else is demo UI/menu logic (not part of the SDK).
#pragma once
#include "../common/camera_setup.h"
#include "../common/i18n.h"
#include <algorithm>
#include <iomanip>
#include <iostream>

// Global state (declared in main.cc)
extern std::shared_ptr<ins_camera::Camera> g_cam;
extern ins_camera::CameraType g_cameraType;

namespace capture_settings {

// --- helpers ---
static bool hasIq3AMode() {
    auto fmode = g_cam->GetCurrentFunctionMode();

    auto vals = g_cam->GetSupportedAttrValues(fmode, "iq_3a_mode");
    return !vals.empty();
}

static void printExposure(const std::shared_ptr<ins_camera::ExposureSettings>& s) {
    if (!s)
        return;
    std::cout << "EV     : " << (s->EVBias() / 20.0) << std::endl;
    std::cout << "ISO    : " << s->Iso() << std::endl;
    std::cout << "shutter: " << s->ShutterSpeed() << "s" << std::endl;
    std::cout << "mode   : " << static_cast<int>(s->ExposureMode()) << std::endl;
    std::cout << "videoISOLimit: " << s->VideoISOTopLimit() << std::endl;
}

static std::string functionModeName(ins_camera::CameraFunctionMode m) {
    auto toName = [](ins_camera::CameraFunctionMode m2) -> const char* {
        switch (m2) {
        case ins_camera::CameraFunctionMode::FUNCTION_MODE_NORMAL_IMAGE: return "PHOTO_SINGLE";
        case ins_camera::CameraFunctionMode::FUNCTION_MODE_HDR_IMAGE: return "PHOTO_HDR";
        case ins_camera::CameraFunctionMode::FUNCTION_MODE_BURST: return "PHOTO_BURST";
        case ins_camera::CameraFunctionMode::FUNCTION_MODE_INTERVAL_SHOOTING: return "PHOTO_INTERVAL";
        case ins_camera::CameraFunctionMode::FUNCTION_MODE_STARLAPSE_IMAGE: return "PHOTO_STARLAPSE";
        case ins_camera::CameraFunctionMode::FUNCTION_MODE_NORMAL_VIDEO: return "VIDEO_NORMAL";
        case ins_camera::CameraFunctionMode::FUNCTION_MODE_HDR_VIDEO: return "VIDEO_HDR";
        case ins_camera::CameraFunctionMode::FUNCTION_MODE_TIMESHIFT: return "VIDEO_TIMESHIFT";
        case ins_camera::CameraFunctionMode::FUNCTION_MODE_STATIC_TIMELAPSE: return "VIDEO_TIMELAPSE";
        case ins_camera::CameraFunctionMode::FUNCTION_MODE_MOBILE_TIMELAPSE: return "VIDEO_TIMELAPSE";
        case ins_camera::CameraFunctionMode::FUNCTION_MODE_LOOP_RECORDING_VIDEO: return "VIDEO_LOOPRECORDING";
        case ins_camera::CameraFunctionMode::FUNCTION_MODE_PURE_VIDEO: return "VIDEO_PURE";
        case ins_camera::CameraFunctionMode::FUNCTION_MODE_SUPER_VIDEO: return "VIDEO_SUPER";
        case ins_camera::CameraFunctionMode::FUNCTION_MODE_BULLETTIME: return "VIDEO_BULLETTIME";
        case ins_camera::CameraFunctionMode::FUNCTION_MODE_DASH_CAM: return "VIDEO_DASHCAM";
        case ins_camera::CameraFunctionMode::FUNCTION_MODE_SELFIE_VIDEO: return "VIDEO_SELFIE";
        case ins_camera::CameraFunctionMode::FUNCTION_MODE_PTZ: return "VIDEO_GIMBAL";
        default: return nullptr;
        }
    };
    const char* name = toName(m);
    if (name)
        return name;
    // Show raw enum value for unmapped modes (helps diagnose new/missing mode mappings)
    static char buf[32];
    snprintf(buf, sizeof(buf), "UNKNOWN(%d)", static_cast<int>(m));
    return buf;
}

// Capability table has no entries for this attr in the current mode: the mode
// genuinely doesn't support this setting (e.g. BulletTime has no ISO/shutter).
// Show the current mode name so the user knows why, per请求.
static void printNoCapForMode(ins_camera::CameraFunctionMode fmode) {
    std::cout << demo_common::L("当前模式 [", "Current mode [") << functionModeName(fmode)
              << demo_common::L("] 不支持该设置", "] does not support this setting") << std::endl;
}

static const char* exposureModeName(ins_camera::PhotographyOptions_ExposureMode m) {
    switch (m) {
    case ins_camera::PhotographyOptions_ExposureMode::AUTO: return "AUTO";
    case ins_camera::PhotographyOptions_ExposureMode::ISO_PRIORITY: return "ISO_PRIORITY";
    case ins_camera::PhotographyOptions_ExposureMode::SHUTTER_PRIORITY: return "SHUTTER_PRIORITY";
    case ins_camera::PhotographyOptions_ExposureMode::MANUAL: return "MANUAL";
    case ins_camera::PhotographyOptions_ExposureMode::ADAPTIVE: return "ADAPTIVE";
    case ins_camera::PhotographyOptions_ExposureMode::FULL_AUTO: return "FULL_AUTO";
    default: return "?";
    }
}

// 设 ISO/快门前确保处于可生效的模式:若当前模式不在 allowed 里,自动切到 target 并提示
// "从 X 切到 Y"。已在允许模式(如用户先手动切了 ISO_PRIORITY)则不动,兼容手动切换。
static void ensureExposureMode(const std::shared_ptr<ins_camera::ExposureSettings>& es,
                               std::initializer_list<ins_camera::PhotographyOptions_ExposureMode> allowed,
                               ins_camera::PhotographyOptions_ExposureMode target) {
    auto cur = es->ExposureMode();
    for (auto a : allowed)
        if (cur == a)
            return;
    std::cout << demo_common::L("曝光模式自动切换: ", "exposure mode auto-switch: ") << exposureModeName(cur) << " -> "
              << exposureModeName(target) << std::endl;
    es->SetExposureMode(target);
}

// --- cmd functions ---
static void cmdInCameraStitching() {
    auto fmode = g_cam->GetCurrentFunctionMode();
    auto vals = g_cam->GetSupportedAttrValues(fmode, "splicing_base_enable");
    if (vals.empty()) {
        std::cout << demo_common::L("当前模式不支持机内拼接", "In-camera stitching not supported in current mode")
                  << std::endl;
        return;
    }
    int enable = 0;
    std::cout << demo_common::L("机内拼接:启用(1)/禁用(0)", "In-camera stitching: Enable(1)/Disable(0)?") << std::endl;
    std::cin >> enable;
    if (g_cam->EnableInCameraStitching(enable ? true : false))
        std::cout << demo_common::L("成功", "OK") << std::endl;
    else
        std::cerr << demo_common::L("失败", "Failed") << std::endl;
}

static void cmdSetActiveSensor() {
    int sensor_device;
    std::cout << "FRONT: 1\nREAR: 2\nALL: 3\n" << demo_common::L("请输入", "enter") << ": ";
    std::cin >> sensor_device;
    if (sensor_device < 1 || sensor_device > 3) {
        std::cout << demo_common::L("无效", "invalid") << std::endl;
        return;
    }
    auto ret = g_cam->SetActiveSensor(static_cast<ins_camera::SensorDevice>(sensor_device));
    std::cout << (ret ? demo_common::L("成功", "OK") : demo_common::L("失败", "Failed")) << std::endl;
}

static void cmdSetSharpness() {
    std::cout << demo_common::L("该功能暂不可用", "Note: this feature is temporarily unavailable") << std::endl;
    if (hasIq3AMode()) {
        const auto fmode = g_cam->GetCurrentFunctionMode();
        std::cout << demo_common::L("锐度: 1=低 2=中(默认) 3=高", "1=Low 2=Medium 3=High") << std::endl;
        int level = 2;
        if (!(std::cin >> level)) {
            std::cin.clear();
            std::cin.ignore(0x7fffffff, '\n');
            return;
        }
        if (level < 1 || level > 3) {
            std::cout << demo_common::L("无效", "invalid") << std::endl;
            return;
        }
        auto cs = g_cam->GetCaptureSettings(fmode);
        if (!cs) {
            std::cout << demo_common::L("获取失败", "get failed") << std::endl;
            return;
        }
        cs->SetValue(ins_camera::CaptureSettings::SettingsType::CaptureSettings_Sharpness, level);
        if (!g_cam->SetCaptureSettings(fmode, cs)) {
            std::cout << demo_common::L("设置失败", "set failed") << std::endl;
            return;
        }
        auto after = g_cam->GetCaptureSettings(fmode);
        int v = after ? after->GetIntValue(ins_camera::CaptureSettings::SettingsType::CaptureSettings_Sharpness) : -1;
        std::cout << (v == level ? demo_common::L("成功", "OK") : demo_common::L("可能未生效", "may not apply")) << " ("
                  << v << ")" << std::endl;
        return;
    }
    int level = 1;
    std::cout << "[Low->0] [Medium->1] [High->2]\n" << demo_common::L("请输入", "enter") << ": ";
    if (!(std::cin >> level)) {
        std::cin.clear();
        std::cin.ignore(0x7fffffff, '\n');
        return;
    }
    if (level < 0 || level > 2) {
        std::cout << demo_common::L("无效", "invalid") << std::endl;
        return;
    }
    bool ok = g_cam->SetGlobalSharpness(static_cast<ins_camera::SharpnessLevel>(level));
    std::cout << (ok ? demo_common::L("成功", "OK") : demo_common::L("失败", "Failed")) << std::endl;
}

static void cmdGetSharpness() {
    std::cout << demo_common::L("该功能暂不可用", "Note: this feature is temporarily unavailable") << std::endl;
    if (hasIq3AMode()) {
        auto cs = g_cam->GetCaptureSettings(g_cam->GetCurrentFunctionMode());
        if (!cs) {
            std::cout << demo_common::L("获取失败", "get failed") << std::endl;
            return;
        }
        int v = cs->GetIntValue(ins_camera::CaptureSettings::SettingsType::CaptureSettings_Sharpness);
        std::cout << demo_common::L("锐度(1~3)", "sharpness(1~3)") << ": " << v << std::endl;
        return;
    }
    ins_camera::SharpnessLevel level;
    if (!g_cam->GetGlobalSharpness(level)) {
        std::cout << demo_common::L("失败", "Failed") << std::endl;
        return;
    }
    const char* name = (level == ins_camera::SharpnessLevel::Low)      ? "Low"
                       : (level == ins_camera::SharpnessLevel::Medium) ? "Medium"
                                                                       : "High";
    std::cout << "sharpness: " << name << std::endl;
}

static int pickFromStrings(const std::vector<std::string>& vals, const std::string& label) {
    const int COLS = 5;
    // Find max display width: "[idx] value"
    int maxW = 0;
    for (size_t i = 0; i < vals.size(); ++i) {
        int w = static_cast<int>(std::to_string(i).size()) + vals[i].size() + 4; // "[i] "
        if (w > maxW)
            maxW = w;
    }
    maxW = std::max(maxW, 6) + 2; // padding between columns
    for (size_t i = 0; i < vals.size(); ++i) {
        std::string item = "[" + std::to_string(i) + "] " + vals[i];
        std::cout << "  " << std::left << std::setw(maxW) << item;
        if ((i + 1) % COLS == 0 || i == vals.size() - 1)
            std::cout << std::endl;
    }
    std::cout << "  [-1] " << demo_common::L("取消", "cancel") << std::endl;
    std::cout << label << ": ";
    int idx;
    if (!(std::cin >> idx)) {
        std::cin.clear();
        std::cin.ignore(0x7fffffff, '\n');
        return -1;
    }
    return (idx >= 0 && idx < static_cast<int>(vals.size())) ? idx : -1;
}

static void cmdSetEV() {
    auto fmode = g_cam->GetCurrentFunctionMode();
    auto es = g_cam->GetExposureSettings(fmode);
    printExposure(es);
    auto vals = g_cam->GetSupportedAttrValues(fmode, "exposure_bias");
    if (vals.empty()) {
        printNoCapForMode(fmode);
        return;
    }
    int idx = pickFromStrings(vals, demo_common::L("选择 EV", "choose EV"));
    if (idx < 0)
        return;
    // Capability values are actual EV (e.g. "-3.7"), proto encoding is EV*20
    double ev = std::stod(vals[idx]) * 20.0;
    int bias = static_cast<int>(ev + (ev >= 0 ? 0.5 : -0.5));
    // EV 补偿仅在自动曝光(AUTO / FULL_AUTO)下有意义;不在则自动切 AUTO 并提示。
    ensureExposureMode(
        es, {ins_camera::PhotographyOptions_ExposureMode::AUTO, ins_camera::PhotographyOptions_ExposureMode::FULL_AUTO},
        ins_camera::PhotographyOptions_ExposureMode::AUTO);
    es->SetEVBias(bias);
    if (g_cam->SetExposureSettings(fmode, es)) {
        es = g_cam->GetExposureSettings(fmode);
        std::cout << demo_common::L("成功", "OK") << " EV=" << vals[idx] << std::endl;
    }
}

static void cmdSetISO() {
    auto fmode = g_cam->GetCurrentFunctionMode();
    auto es = g_cam->GetExposureSettings(fmode);
    printExposure(es);
    // SDK auto-switches exposure mode when setting ISO
    auto vals = g_cam->GetSupportedAttrValues(fmode, "exposure_iso");
    // Filter out "0" (auto)
    std::vector<std::string> filtered;
    for (auto& v : vals)
        if (v != "0")
            filtered.push_back(v);
    if (filtered.empty()) {
        printNoCapForMode(fmode);
        return;
    }
    int idx = pickFromStrings(filtered, demo_common::L("选择 ISO", "choose ISO"));
    if (idx < 0)
        return;
    int iso = std::stoi(filtered[idx]);
    // ISO 仅在 MANUAL / ISO_PRIORITY 生效;不在则自动切 MANUAL 并提示(可先手动切 ISO_PRIORITY)。
    ensureExposureMode(es,
                       {ins_camera::PhotographyOptions_ExposureMode::MANUAL,
                        ins_camera::PhotographyOptions_ExposureMode::ISO_PRIORITY},
                       ins_camera::PhotographyOptions_ExposureMode::MANUAL);
    es->SetIso(iso);
    if (g_cam->SetExposureSettings(fmode, es)) {
        es = g_cam->GetExposureSettings(fmode);
        std::cout << demo_common::L("成功", "OK") << " ISO=" << es->Iso() << std::endl;
    }
}

static void cmdSetShutter() {
    auto fmode = g_cam->GetCurrentFunctionMode();
    auto es = g_cam->GetExposureSettings(fmode);
    printExposure(es);
    // SDK auto-switches exposure mode when setting shutter
    auto vals = g_cam->GetSupportedAttrValues(fmode, "exposure_shutter_speed");
    if (vals.empty()) {
        printNoCapForMode(fmode);
        return;
    }
    int idx = pickFromStrings(vals, demo_common::L("选择快门", "choose shutter"));
    if (idx < 0)
        return;
    // Parse fraction string like "1/60" → 0.01666...
    auto parseFraction = [](const std::string& s) -> double {
        auto pos = s.find('/');
        if (pos != std::string::npos)
            return std::stod(s.substr(0, pos)) / std::stod(s.substr(pos + 1));
        return std::stod(s);
    };
    double speed = parseFraction(vals[idx]);
    // 快门仅在 MANUAL / SHUTTER_PRIORITY 生效;不在则自动切 MANUAL 并提示(可先手动切 SHUTTER_PRIORITY)。
    ensureExposureMode(es,
                       {ins_camera::PhotographyOptions_ExposureMode::MANUAL,
                        ins_camera::PhotographyOptions_ExposureMode::SHUTTER_PRIORITY},
                       ins_camera::PhotographyOptions_ExposureMode::MANUAL);
    // ONE X 固件特性:手动快门需与有效 ISO 成对才生效;ISO 为 0(自动)时单设快门不会生效,
    // 须先设一个具体 ISO(菜单6)。其他机型无此限制。
    if (g_cameraType == ins_camera::CameraType::Insta360OneX && es->Iso() <= 0) {
        std::cout << demo_common::L(
            "[ONE X 提示] 当前 ISO 为自动,单独设快门可能不生效,请先用菜单6 设置一个具体 ISO 值再设快门。",
            "[ONE X] ISO is auto; setting shutter alone may not take effect. Set a concrete ISO (menu 6) first.")
                  << std::endl;
    }
    es->SetShutterSpeed(speed);
    if (g_cam->SetExposureSettings(fmode, es)) {
        es = g_cam->GetExposureSettings(fmode);
        std::cout << demo_common::L("成功", "OK") << " shutter=" << vals[idx] << "s" << std::endl;
    }
}

static void cmdSetWB() {
    auto fmode = g_cam->GetCurrentFunctionMode();
    auto cs = g_cam->GetCaptureSettings(fmode);
    if (cs)
        std::cout << "WB: " << cs->GetIntValue(ins_camera::CaptureSettings::SettingsType::CaptureSettings_WhiteBalance)
                  << std::endl;
    auto vals = g_cam->GetSupportedAttrValues(fmode, "white_balance");
    if (vals.empty()) {
        printNoCapForMode(fmode);
        return;
    }
    // 保留 "0"=自动(白平衡的 0 是合法选项，与 ISO 不同)，仅把显示名换成"自动/Auto"。
    std::vector<std::string> display;
    display.reserve(vals.size());
    for (auto& v : vals)
        display.push_back(v == "0" ? demo_common::L("自动", "Auto") : v);
    int idx = pickFromStrings(display, demo_common::L("选择白平衡", "choose WB"));
    if (idx < 0)
        return;
    int wb = std::stoi(vals[idx]);
    cs->SetValue(ins_camera::CaptureSettings::SettingsType::CaptureSettings_WhiteBalance, wb);
    if (g_cam->SetCaptureSettings(fmode, cs)) {
        cs = g_cam->GetCaptureSettings(fmode);
        if (cs)
            std::cout << demo_common::L("成功", "OK") << " WB="
                      << cs->GetIntValue(ins_camera::CaptureSettings::SettingsType::CaptureSettings_WhiteBalance)
                      << std::endl;
    }
}

static void cmdSetExposureMode() {
    auto fmode = g_cam->GetCurrentFunctionMode();
    auto es = g_cam->GetExposureSettings(fmode);
    printExposure(es);
    auto vals = g_cam->GetSupportedAttrValues(fmode, "exposure_program");
    if (vals.empty()) {
        // Fallback: hardcoded list if capability table unavailable
        vals = {"AUTO", "ISO_PRIORITY", "SHUTTER_PRIORITY", "MANUAL", "ADAPTIVE", "FULL_AUTO"};
    }
    int idx = pickFromStrings(vals, demo_common::L("选择曝光模式", "choose exposure mode"));
    if (idx < 0)
        return;
    int mode = g_cam->GetAttrValueByName("exposure_program", vals[idx]);
    if (mode < 0) {
        std::cout << demo_common::L("未知模式", "unknown mode") << std::endl;
        return;
    }
    es->SetExposureMode(static_cast<ins_camera::PhotographyOptions_ExposureMode>(mode));
    if (g_cam->SetExposureSettings(fmode, es)) {
        es = g_cam->GetExposureSettings(fmode);
        std::cout << demo_common::L("成功", "OK") << " mode=" << es->ExposureMode() << " (" << vals[idx] << ")"
                  << std::endl;
    }
}

static void cmdResetToAuto() {
    // 一键恢复自动。实测:把 EV/WB 和"切 AUTO"合到一次下发时,相机会忽略 EV/WB
    // (ISO/快门能成)。而单独设 EV / 单独设 WB 都能成。故拆成三次独立下发,每次
    // 都重新 Get 最新状态、只改一项,复刻已验证可用的单设路径。
    auto fmode = g_cam->GetCurrentFunctionMode();
    bool ok = true;

    // 1) 曝光模式 AUTO(SetExposureMode(AUTO) 内部已把 ISO/快门归 0)
    if (auto es = g_cam->GetExposureSettings(fmode)) {
        es->SetExposureMode(ins_camera::PhotographyOptions_ExposureMode::AUTO);
        if (!g_cam->SetExposureSettings(fmode, es)) {
            ok = false;
            std::cout << demo_common::L("曝光模式设置失败", "exposure mode set failed") << std::endl;
        }
    }

    // 2) 单独设 EV = 0(重新 Get,不携带上一步的批量改动)
    if (auto es = g_cam->GetExposureSettings(fmode)) {
        es->SetEVBias(0);
        if (!g_cam->SetExposureSettings(fmode, es)) {
            ok = false;
            std::cout << demo_common::L("EV 设置失败", "EV set failed") << std::endl;
        }
    }

    // 3) 单独设白平衡 = 0(自动)
    if (auto cs = g_cam->GetCaptureSettings(fmode)) {
        cs->SetValue(ins_camera::CaptureSettings::SettingsType::CaptureSettings_WhiteBalance, 0);
        if (!g_cam->SetCaptureSettings(fmode, cs)) {
            ok = false;
            std::cout << demo_common::L("白平衡设置失败", "WB set failed") << std::endl;
        }
    }

    auto es = g_cam->GetExposureSettings(fmode);
    std::cout << (ok ? demo_common::L("已恢复自动(曝光/ISO/快门/EV/白平衡)",
                                      "Reset to AUTO (exposure/ISO/shutter/EV/WB)")
                     : demo_common::L("部分项恢复失败(见上)", "some items failed (see above)"))
              << " (mode=" << (es ? es->ExposureMode() : -1) << ")" << std::endl;
}

static void cmdShowCapabilityTable() {
    auto fmode = g_cam->GetCurrentFunctionMode();
    auto attrs = g_cam->GetSupportedAttrNames(fmode);
    if (attrs.empty()) {
        std::cout << demo_common::L("能力表为空", "Capability table empty") << std::endl;
        return;
    }
    bool hasPro = false;
    for (auto& a : attrs)
        if (a == "iq_3a_mode") {
            hasPro = true;
            break;
        }
    std::cout << "\n=== " << attrs.size() << " " << demo_common::L("个属性", "attrs") << " ===" << std::endl;
    if (hasPro)
        std::cout << demo_common::L("[此模式支持普通/专业切换]", "[NORMAL/PRO supported]") << std::endl;

    for (auto& a : attrs) {
        std::cout << "  " << a;
        auto deps = g_cam->GetAttrDependOn(fmode, a);
        if (!deps.empty()) {
            std::cout << " (";
            for (size_t i = 0; i < deps.size(); ++i) {
                if (i)
                    std::cout << ", ";
                std::cout << deps[i];
            }
            std::cout << ")";

            // Enumerate all dependency contexts to show complete value set.
            // For iq_3a_mode: show NORMAL / PRO branches.
            // For other single-level deps: iterate over dep's own values.
            // For multi-level deps: iterate first dep × second dep (up to 12 combos).
            if (deps.size() == 1 && deps[0] == "iq_3a_mode") {
                // Capability table: "default" branch = PRO, "NORMAL" branch = NORMAL
                auto nv = g_cam->GetSupportedAttrValues(fmode, a, "NORMAL");
                auto pv = g_cam->GetSupportedAttrValues(fmode, a, "");
                std::cout << "\n      " << demo_common::L("普通", "NORMAL") << ":";
                for (auto& v : nv)
                    std::cout << " " << v;
                std::cout << "\n      " << demo_common::L("专业", "PRO") << ":";
                for (auto& v : pv)
                    std::cout << " " << v;
            } else {
                auto d0_vals = g_cam->GetSupportedAttrValues(fmode, deps[0]);
                int combo = 0;
                for (auto& d0 : d0_vals) {
                    if (deps.size() == 1) {
                        auto vals = g_cam->GetSupportedAttrValues(fmode, a, d0);
                        if (!vals.empty()) {
                            std::cout << "\n        " << deps[0] << "=" << d0 << ":";
                            for (auto& v : vals)
                                std::cout << " " << v;
                        }
                    } else {
                        auto d1_vals = g_cam->GetSupportedAttrValues(fmode, deps[1]);
                        for (auto& d1 : d1_vals) {
                            if (combo++ >= 12)
                                break;
                            std::string ctx = d0 + "|" + d1;
                            auto vals = g_cam->GetSupportedAttrValues(fmode, a, ctx);
                            if (!vals.empty()) {
                                std::cout << "\n        " << deps[0] << "=" << d0 << " " << deps[1] << "=" << d1 << ":";
                                for (auto& v : vals)
                                    std::cout << " " << v;
                            }
                        }
                    }
                    if (combo >= 12) {
                        std::cout << " ...";
                        break;
                    }
                }
            }
        } else {
            auto vals = g_cam->GetSupportedAttrValues(fmode, a);
            if (!vals.empty()) {
                std::cout << " (" << vals.size() << ") [";
                int n = std::min((int)vals.size(), 8);
                for (int i = 0; i < n; ++i) {
                    if (i)
                        std::cout << " ";
                    std::cout << vals[i];
                }
                if ((int)vals.size() > 8)
                    std::cout << " ...";
                std::cout << "]";
            }
        }
        std::cout << std::endl;
    }
}

static void cmdToggleIq3AMode() {
    if (!hasIq3AMode()) {
        std::cout << demo_common::L("当前模式不支持", "not supported in current mode") << std::endl;
        return;
    }
    auto fmode = g_cam->GetCurrentFunctionMode();
    auto cs = g_cam->GetCaptureSettings(fmode);
    if (!cs) {
        std::cout << demo_common::L("获取失败", "get failed") << std::endl;
        return;
    }
    int cur = cs->GetIntValue(ins_camera::CaptureSettings::SettingsType::CaptureSettings_Iq3AMode);
    const char* curName = (cur == 2) ? "PRO" : "NORMAL";
    int target = (cur == 2) ? 1 : 2; // 1=NORMAL, 2=PRO
    const char* targetName = (target == 2) ? "PRO" : "NORMAL";
    std::cout << demo_common::L("当前", "Current") << ": " << curName << " -> " << targetName << std::endl;
    cs->SetValue(ins_camera::CaptureSettings::SettingsType::CaptureSettings_Iq3AMode, target);
    if (g_cam->SetCaptureSettings(fmode, cs)) {
        std::cout << demo_common::L("已切换", "Switched") << ": " << targetName << std::endl;
    } else {
        std::cout << demo_common::L("切换失败", "Failed") << std::endl;
    }
}

// --- main menu loop ---
inline void run() {
    while (true) {
        auto fmode = g_cam->GetCurrentFunctionMode();
        std::cout << "\n=== " << demo_common::L("当前", "Current") << ": " << functionModeName(fmode)
                  << " ===" << std::endl;
        std::cout << demo_common::L("曝光与设置", "Capture Settings") << ":\n"
                  << "  1: " << demo_common::L("切换镜头", "switch lens") << "\n"
                  << "  2: " << demo_common::L("机内拼接开关", "toggle stitching") << "\n"
                  << "  3: " << demo_common::L("设锐度", "set sharpness") << "\n"
                  << "  4: " << demo_common::L("取锐度", "get sharpness") << "\n"
                  << "  5: " << demo_common::L("设 EV", "set EV") << "\n"
                  << "  6: " << demo_common::L("设 ISO", "set ISO") << "\n"
                  << "  7: " << demo_common::L("设快门", "set shutter") << "\n"
                  << "  8: " << demo_common::L("设白平衡", "set WB") << "\n"
                  << "  9: " << demo_common::L("设曝光模式", "set exposure mode") << "\n"
                  << " 10: " << demo_common::L("进阶: 能力表", "Adv: capability table") << "\n"
                  << " 12: " << demo_common::L("一键恢复自动", "one-tap reset to AUTO") << "\n";
        if (hasIq3AMode())
            std::cout << " 11: " << demo_common::L("切换普通/专业模式", "toggle NORMAL/PRO") << "\n";
        std::cout << "  0: " << demo_common::L("返回", "back") << "\n" << demo_common::L("输入", "enter") << ": ";
        int opt = -1;
        if (!(std::cin >> opt)) {
            std::cin.clear();
            std::cin.ignore(0x7fffffff, '\n');
            continue;
        }
        if (opt == 0)
            break;
        switch (opt) {
        case 1: cmdSetActiveSensor(); break;
        case 2: cmdInCameraStitching(); break;
        case 3: cmdSetSharpness(); break;
        case 4: cmdGetSharpness(); break;
        case 5: cmdSetEV(); break;
        case 6: cmdSetISO(); break;
        case 7: cmdSetShutter(); break;
        case 8: cmdSetWB(); break;
        case 9: cmdSetExposureMode(); break;
        case 10: cmdShowCapabilityTable(); break;
        case 11: cmdToggleIq3AMode(); break;
        case 12: cmdResetToAuto(); break;
        default: std::cout << demo_common::L("无效", "invalid") << std::endl; break;
        }
    }
}

} // namespace capture_settings