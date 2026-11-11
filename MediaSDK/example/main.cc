// =============================================================================
// MediaSDKTest — Insta360 MediaSDK Offline Stitching Command-Line Demo
//
// Description:
//   Reads panoramic media files produced by Insta360 cameras (.insv video /
//   .insp image), drives the MediaSDK pipeline to perform stitching,
//   stabilization (FlowState), color grading, and other processing, then
//   writes the result as an .mp4 video or .jpg / .png image.
//
// Compile:
//   Build the executable MediaSDKTest.  It depends on the SDK public header
//   ins_stitcher.h (located under the SDK's include/ directory) and must be
//   linked against the MediaSDK shared library.  See the Developer Guide
//   shipped with the release package for detailed build instructions.
//
// Run:
//   1) Place the models/ directory (from the SDK release package) next to the
//      MediaSDKTest executable.
//   2) Run  MediaSDKTest -help  to see the full list of options.
//   3) Typical usage:
//        MediaSDKTest -inputs <input.insv> -output <out.mp4>
//                     -stitch_type optflow -output_size 3840x1920
//                     -enable_flowstate
//
// Key pipeline steps:
//   InitEnv → SetModelFileRootDir → Create VideoStitcher / ImageStitcher
//   → Configure parameters → Register progress / status callbacks
//   → StartStitch (video, async) or Stitch (image, sync)
// =============================================================================

#include <iostream>
#include <ins_stitcher.h>

#include <iostream>
#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <vector>
#include <sstream>
#include <filesystem>
#include <iomanip>
#include <ctime>

#ifdef _WIN32
#include <direct.h>
#include <Windows.h>
#include <shellapi.h>   // CommandLineToArgvW

// On laptops with dual GPUs (NVIDIA Optimus / AMD PowerXpress), force the
// discrete GPU to be used instead of the integrated GPU.  Without these
// exports the OS may silently route the SDK onto the low-power integrated GPU,
// causing a significant increase in stitching time.
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

#endif // _WIN32


using namespace std::chrono;
using namespace ins;

const std::string helpstr =
"{-help                   | default               | print this message                  }\n"
"{-model_root_dir         | <exe>/models/         | the dir of models (auto-deployed)   }\n"
"{-inputs                 | None                  | input files                         }\n"
"{-output                 | None                  | out path                            }\n"
"{-stitch_type            | optflow               | template                            }\n"
"{                                                | optflow                             }\n"
"{                                                | dynamicstitch                       }\n"
"{                                                | aistitch                            }\n"
"{-bitrate                | same as input video   | the bitrate of ouput file           }\n"
"{-enable_flowstate       | OFF                   | enable flowstate                    }\n"
"{-enable_directionlock   | OFF                   | enable directionlock                }\n"
"{-output_size            | 1920x960              | the resolution of output            }\n"
"{-enable_stitchfusion    | OFF                   | stitch_fusion                       }\n"
"{-enable_denoise         | OFF                   | enable denoise                      }\n"
"{-enable_colorplus       | OFF                   | enable colorplus                    }\n"
"{-enable_coolingshell    | OFF                   | enable Cooling Shell Detection      }\n"
"{-enable_deflicker       | OFF                   | enable deflicker                    }\n"
"{-enable_defringe        | OFF                   | enable defringe (skipped for X6)    }\n"
"{-image_sequence_dir     | None                  | the output dir of image sequence    }\n"
"{-image_type             | jpg                   | jpg                                 }\n"
"{                                                | png                                 }\n"
"{-export_frame_index     |                       | Derived frame number sequence       }\n"
"{                                                | example: 20-50-30                   }\n"
"{-camera_accessory_type  | default 0             | refer to 'common.h'                 }\n"
"{-enable_h265_encoder    | h264                  | encode format                       }\n"
"{-enable_10bit           | OFF                   | enable 10-bit export (auto-fallback to 8bit if unsupported)}\n"
"{-disable_cuda           | false                 | disable cuda                        }\n"
"{-enable_soft_encode     | false                 | use soft encoder                    }\n"
"{-enable_soft_decode     | false                 | use soft decoder                    }\n"
"{-image_processing_accel | auto                  | image_processing_accel              }\n"
"{                                                | auto                                }\n"
"{                                                | cpu                                 }\n"
"{-exposure               | 0                     | use exposure[-100,100]              }\n"
"{-highlights             | 0                     | use highlights[-100,100]            }\n"
"{-shadows                | 0                     | use shadows[-100,100]               }\n"
"{-contrast               | 0                     | use contrast[-100,100]              }\n"
"{-brightness             | 0                     | use brightness[-100,100]            }\n"
"{-blackpoint             | 0                     | use blackpoint[-100,100]            }\n"
"{-saturation             | 0                     | use saturation[-100,100]            }\n"
"{-vibrance               | 0                     | use vibrance[-100,100]              }\n"
"{-warmth                 | 0                     | use warmth[-100,100]                }\n"
"{-tint                   | 0                     | use tint[-100,100]                  }\n"
"{-definition             | 0                     | use definition[0,100]               }\n"
"{--debug      | OFF                   | print debug info (same as --log_level verbose)}\n"
"{--log_level  | ERR                   | verbose|info|warning|error|fatal    }\n"
"{--log_file [file]        | <none>                | write SDK log to file; empty=<exe>/logs/ }\n";

// Convert a string from the system's active code page (e.g. GBK on a
// Simplified-Chinese Windows console) to UTF-8.  The SDK expects all path
// arguments in UTF-8; without this conversion, paths containing non-ASCII
// characters (e.g. Chinese directory names) would fail to open.
// On Linux the locale is already UTF-8, so the string is returned unchanged.
// NOTE: main now rebuilds argv directly into UTF-8 via BuildUtf8Args, so this
// has no callers; kept for reference.
[[maybe_unused]] static std::string stringToUtf8(const std::string& original_str) {
#ifdef _WIN32
    const char* std_origin_str = original_str.c_str();
    const int std_origin_str_len = static_cast<int>(original_str.length());

    int nwLen = MultiByteToWideChar(CP_ACP, 0, std_origin_str, -1, NULL, 0);

    wchar_t* pwBuf = new wchar_t[nwLen + 1];
    ZeroMemory(pwBuf, nwLen * 2 + 2);

    MultiByteToWideChar(CP_ACP, 0, std_origin_str, std_origin_str_len, pwBuf, nwLen);

    int nLen = WideCharToMultiByte(CP_UTF8, 0, pwBuf, -1, NULL, NULL, NULL, NULL);

    char* pBuf = new char[nLen + 1];
    ZeroMemory(pBuf, nLen + 1);

    WideCharToMultiByte(CP_UTF8, 0, pwBuf, nwLen, pBuf, nLen, NULL, NULL);

    std::string ret_str(pBuf);

    delete[] pwBuf;
    delete[] pBuf;

    pwBuf = NULL;
    pBuf = NULL;

    return ret_str;
#else
    return original_str;
#endif
}

#ifdef _WIN32
// On Windows the argv passed to main() is narrow-char and depends on the console
// code page, so non-ASCII paths often become "?" (irreversibly), leaving the SDK
// with a broken path. This function reads the original Unicode command line via
// GetCommandLineW and rebuilds argv as UTF-8, so all arguments (especially
// non-ASCII paths) are valid UTF-8 without needing per-argument stringToUtf8.
// The returned vector<string> owns the storage that argv points into; both must
// stay alive for the duration of main.
static std::vector<std::string> BuildUtf8Args(std::vector<char*>& out_argv) {
    std::vector<std::string> storage;
    int wargc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (!wargv) {
        return storage;
    }
    storage.reserve(wargc);
    for (int i = 0; i < wargc; ++i) {
        int len = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
        std::string s(len > 0 ? len - 1 : 0, '\0');
        if (len > 0) {
            WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, s.data(), len, nullptr, nullptr);
        }
        storage.push_back(std::move(s));
    }
    LocalFree(wargv);
    out_argv.clear();
    for (auto& s : storage) {
        out_argv.push_back(s.data());
    }
    return storage;
}
#endif

// Clamp val to [min_val, max_val].  Used to restrict color-adjustment
// parameters to the range accepted by the SDK before passing them in.
template<typename T>
T clamp(const T& val, const T& min_val, const T& max_val) {
    return std::min(std::max(val, min_val), max_val);
}

// Split string s on the given delimiter character and return the resulting
// tokens.  Used to parse compound arguments such as "1920x960" (output size)
// and "20-50-30" (frame-index export sequence).
std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);

    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

#ifdef _WIN32
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

// Return the directory that contains the current executable (with a trailing
// path separator).  This is used to resolve the default models/ directory
// relative to the executable so that double-clicking the binary — without
// setting a working directory — still locates the model files correctly.
std::string getExecutablePath() {
    std::string dir;
#ifdef _WIN32
    // Use the wide-char API and convert to UTF-8 so that executable paths
    // containing non-ASCII characters (e.g. Chinese directory names) are
    // encoded consistently with the rest of the program.  GetModuleFileNameA
    // returns the path in the system ANSI code page (GBK on zh-CN); feeding
    // those bytes to std::filesystem::u8path (used by the default --log_file
    // logs/ branch) yields invalid UTF-8 and throws, silently terminating.
    wchar_t wbuffer[1024] = {0};
    GetModuleFileNameW(NULL, wbuffer, 1024);
    int len = WideCharToMultiByte(CP_UTF8, 0, wbuffer, -1, nullptr, 0, nullptr, nullptr);
    std::string path(len > 0 ? len - 1 : 0, '\0');
    if (len > 0) {
        WideCharToMultiByte(CP_UTF8, 0, wbuffer, -1, path.data(), len, nullptr, nullptr);
    }
    dir = path.substr(0, path.find_last_of("\\") + 1);
#else
    // Linux: readlink /proc/self/exe gives the absolute executable path;
    // take everything up to the last '/' as the directory (trailing '/' kept).
    char buffer[4096];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len > 0) {
        buffer[len] = '\0';
        std::string path = buffer;
        dir = path.substr(0, path.find_last_of('/') + 1);
    }
#endif
    return dir;
}

// Return true if path ends with a forward slash or backslash.
// SetModelFileRootDir() requires a trailing separator; this helper is used
// to validate / append the separator before passing the path to the SDK.
bool endsWithSlash(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    return path.back() == '/' || path.back() == '\\';
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    // Set the console output code page to UTF-8 so that paths / messages we
    // print (which are UTF-8 internally, e.g. Chinese directory names) render
    // correctly instead of appearing as garbled GBK bytes.
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::cout<< "MediaSDKTest Insta360 MediaSDK Stitching Demo" << std::endl;
    std::cout<<"SDK version: " << ins::GetVersion() << std::endl;
#ifdef _WIN32
    // Rebuild argv as UTF-8 from the Unicode command line to fix non-ASCII path
    // arguments (see BuildUtf8Args).  utf8_storage must outlive argv, so it is
    // declared in main scope.
    std::vector<char*> utf8_argv;
    std::vector<std::string> utf8_storage = BuildUtf8Args(utf8_argv);
    if (!utf8_argv.empty()) {
        argc = static_cast<int>(utf8_argv.size());
        argv = utf8_argv.data();
    }
#endif
    // Default to ERR-level logging to keep the console quiet during normal
    // operation.  Pass --debug on the command line to switch to
    // VERBOSE for detailed SDK diagnostics.
    ins::SetLogLevel(ins::InsLogLevel::ERR);

    // Initialize the SDK runtime (GPU context, thread pools, codec registry,
    // etc.).  All other SDK calls must be made after InitEnv returns.
    ins::InitEnv();

    // Default model root directory: models/ subdirectory next to the
    // executable.  AI stitching, ColorPlus, and denoise features all require
    // their respective .ins model files to be present under this path.
    // Override with -model_root_dir on the command line if needed.
    std::string model_root_dir;
#ifdef _WIN32
    model_root_dir = getExecutablePath() + std::string("models\\");
#else
    model_root_dir = getExecutablePath() + std::string("models/");
#endif
    
    std::vector<std::string> input_paths;
    std::string output_path;
    std::string image_sequence_dir;
    std::string exported_frame_number_sequence;

    STITCH_TYPE stitch_type = STITCH_TYPE::OPTFLOW;
    IMAGE_TYPE image_type = IMAGE_TYPE::JPEG;
    CameraAccessoryType accessory_type = CameraAccessoryType::kNormal;
    ImageProcessingAccel  image_processing_accel = ImageProcessingAccel::kAuto;
    int output_width = 1920;
    int output_height = 960;
    int output_bitrate = 0;
    bool enable_log_file = false;  // whether --log_file was given (controls file logging)
    std::string log_path;          // --log_file value; empty + enable_log_file=true uses default logs/

    bool enable_flowstate = false;
    bool enable_cuda = true;
    bool enable_soft_encode = false;
    bool enable_soft_decode = false;
    bool enalbe_stitchfusion = false;
    bool enable_colorplus = false;
    bool enable_directionlock = false;
    bool enable_sequence_denoise = false;
    bool enable_H265_encoder = false;
    bool enable_10bit = false;
    bool enable_deflicker = false;
    bool enable_defringe = false;
    bool enable_cooling_shell_detection = false;

    // Color / ISP adjustment parameters.  Valid range is [-100, 100] for all
    // fields except definition which is [0, 100].  All values default to 0,
    // meaning no additional color processing is applied.
    int exposure = 0;
    int highlights = 0;
    int shadows = 0;
    int contrast = 0;
    int brightness = 0;
    int blackpoint = 0;
    int saturation = 0;
    int vibrance = 0;
    int warmth = 0;
    int tint = 0;
    int definition = 0;

    // --------------------------------------------------------------------------
    // Command-line argument parsing.
    // Run  MediaSDKTest -help  to see the full parameter reference (helpstr).
    // --------------------------------------------------------------------------
    // Return the value of the current option (argv[i+1]) and advance i.  If the
    // value is missing (option at end of command line with nothing after it),
    // reading argv[argc] and constructing a std::string from it is undefined
    // behavior (observed to hang/crash), so check explicitly and exit with an error.
    auto next_arg = [&](int& i) -> std::string {
        if (i + 1 >= argc) {
            std::cerr << "error: option '" << argv[i] << "' requires a value" << std::endl;
            std::exit(1);
        }
        return std::string(argv[++i]);
    };
    for (int i = 1; i < argc; i++) {
        // -inputs <file1> [file2 ...]
        // One or more input files.  Multiple files are required for multi-lens
        // cameras where each lens is stored in a separate file.
        if (std::string("-inputs") == std::string(argv[i])) {
            std::string input_path = next_arg(i);
            while (input_path[0] != '-') {
                // argv is already UTF-8 (rebuilt via BuildUtf8Args on Windows, native on Linux).
                input_paths.push_back(input_path);
                if (i + 1 >= argc) break;   // reached the end, stop consuming to avoid overrun
                input_path = std::string(argv[++i]);
            }
        }
        // -output <path>
        // Output file path for the stitched video or image.
        if (std::string("-output") == std::string(argv[i])) {
            output_path = next_arg(i);
        }
        // -model_root_dir <dir>
        // Override the default model directory (must end with a path separator).
        else if (std::string("-model_root_dir") == std::string(argv[i])) {
            model_root_dir = next_arg(i);
        }
        // -enable_coolingshell
        // Enable detection of the Cooling Shell accessory on supported cameras.
        else if (std::string("-enable_coolingshell") == std::string(argv[i])) {
            enable_cooling_shell_detection = true;
        }
        // -stitch_type <template|optflow|dynamicstitch|aistitch>
        // Select the stitching algorithm.  optflow (optical flow) is the
        // default and offers the best seam quality.  aistitch uses a neural
        // network and requires the AI model files to be present.
        else if (std::string("-stitch_type") == std::string(argv[i])) {
            std::string stitchType = next_arg(i);
            if (stitchType == std::string("optflow")) {
                stitch_type = STITCH_TYPE::OPTFLOW;
            }
            else if (stitchType == std::string("dynamicstitch")) {
                stitch_type = STITCH_TYPE::DYNAMICSTITCH;
            }
            else if (stitchType == std::string("aistitch")) {
                stitch_type = STITCH_TYPE::AIFLOW;
            }
        }
        // -image_processing_accel <auto|cpu>
        // Control the accelerator used for image-processing operations.
        // "auto" lets the SDK choose (GPU when available); "cpu" forces
        // software-only processing.
        else if (std::string("-image_processing_accel") == std::string(argv[i])) {
            std::string imageProcessingAccel = next_arg(i);
            if (imageProcessingAccel == std::string("auto")) {
                image_processing_accel = ImageProcessingAccel::kAuto;
            }
            else if (imageProcessingAccel == std::string("cpu")) {
                image_processing_accel = ImageProcessingAccel::kCPU;
            }
        }
        // -enable_flowstate
        // Enable FlowState electronic image stabilization (gyroscope-based).
        else if (std::string("-enable_flowstate") == std::string(argv[i])) {
            enable_flowstate = true;
        }
        // -disable_cuda
        // Disable CUDA GPU acceleration; the SDK will fall back to CPU-only
        // processing.  Useful for debugging or on machines without a supported GPU.
        else if (std::string("-disable_cuda") == std::string(argv[i])) {
            enable_cuda = false;
        }
        // -enable_stitchfusion
        // Enable stitch-fusion post-processing to reduce visible seam artifacts.
        else if (std::string("-enable_stitchfusion") == std::string(argv[i])) {
            enalbe_stitchfusion = true;
        }
        // -enable_denoise
        // Enable temporal sequence denoising (reduces noise across frames).
        else if (std::string("-enable_denoise") == std::string(argv[i])) {
            enable_sequence_denoise = true;
        }
        // -enable_colorplus
        // Enable ColorPlus automatic color enhancement (requires model files).
        else if (std::string("-enable_colorplus") == std::string(argv[i])) {
            enable_colorplus = true;
        }
        // -enable_directionlock
        // Lock the output viewing direction so it does not drift during
        // stabilization; keeps the horizon level at the cost of some FOV.
        else if (std::string("-enable_directionlock") == std::string(argv[i])) {
            enable_directionlock = true;
        }
        // -enable_h265_encoder
        // Use H.265 (HEVC) for video encoding instead of the default H.264.
        // Produces smaller files at equivalent quality but requires H.265
        // decoder support on the playback device.
        else if (std::string("-enable_h265_encoder") == std::string(argv[i])) {
            enable_H265_encoder = true;
        }
        // -enable_10bit
        // Request 10-bit video export. Only effective for 10-bit source material
        // (e.g. C9/X6 10-bit footage); 8-bit sources silently fall back to 8-bit.
        // H.264 is automatically switched to H.265 when 10-bit is active.
        else if (std::string("-enable_10bit") == std::string(argv[i])) {
            enable_10bit = true;
        }
        // -bitrate <bps>
        // Target output bitrate in bits per second.  0 means "match input".
        else if (std::string("-bitrate") == std::string(argv[i])) {
            output_bitrate = atoi(next_arg(i).c_str());
        }
        // -output_size <WxH>  (e.g. "3840x1920")
        // Resolution of the stitched output.  Defaults to 1920x960.
        else if (std::string("-output_size") == std::string(argv[i])) {
            auto res = split(next_arg(i), 'x');
            if (res.size() == 2) {
                output_width = std::atoi(res[0].c_str());
                output_height = std::atoi(res[1].c_str());
            }
        }
        // -image_sequence_dir <dir>
        // Output directory for an image-sequence export.  When set, each
        // stitched frame is written as a separate image file instead of a video.
        else if (std::string("-image_sequence_dir") == std::string(argv[i])) {
            // argv is already UTF-8 (see BuildUtf8Args at the top of main); use directly.
            image_sequence_dir = next_arg(i);
        }
        // -image_type <jpg|png>
        // Image format for image-sequence exports and single-image stitching.
        else if (std::string("-image_type") == std::string(argv[i])) {
            std::string type = next_arg(i);
            if (type == std::string("jpg")) {
                image_type = IMAGE_TYPE::JPEG;
            }
            else if (type == std::string("png")) {
                image_type = IMAGE_TYPE::PNG;
            }
        }
        // -camera_accessory_type <int>
        // Integer enum value identifying the camera accessory (e.g. lens guard,
        // dive case).  Refer to CameraAccessoryType in common.h for valid values.
        else if (std::string("-camera_accessory_type") == std::string(argv[i])) {
            accessory_type = static_cast<CameraAccessoryType>(std::atoi(next_arg(i).c_str()));
        }
        // -export_frame_index <seq>  (e.g. "20-50-30")
        // Hyphen-separated list of video frame indices to export as still images
        // when -image_sequence_dir is also specified.  Omit to export all frames.
        else if (std::string("-export_frame_index") == std::string(argv[i])) {
            exported_frame_number_sequence = next_arg(i);
        }
        // -enable_deflicker
        // Enable deflicker processing to reduce frame-to-frame brightness
        // variation caused by rolling-shutter or lighting conditions.
        else if (std::string("-enable_deflicker") == std::string(argv[i])) {
            enable_deflicker = true;
        }
        // -enable_defringe
        // Enable defringe (purple / green fringing removal) around high-contrast
        // edges in the stitched output.
        else if (std::string("-enable_defringe") == std::string(argv[i])) {
            enable_defringe = true;
        }
        // -enable_soft_encode / -enable_soft_decode
        // Force software (CPU-based) encoding or decoding.  By default the SDK
        // uses hardware codecs when available.  Software codecs are slower but
        // may be needed for compatibility on certain systems.
        else if (std::string("-enable_soft_encode") == std::string(argv[i])) {
            enable_soft_encode = true;
        }
        else if (std::string("-enable_soft_decode") == std::string(argv[i])) {
            enable_soft_decode = true;
        }
        // Color / ISP adjustment parameters — each clamped to its valid range.
        else if (std::string("-exposure") == std::string(argv[i])) {
            exposure = atoi(next_arg(i).c_str());
            exposure = clamp<int>(exposure, -100, 100);
        }
        else if (std::string("-highlights") == std::string(argv[i])) {
            highlights = atoi(next_arg(i).c_str());
            highlights = clamp<int>(highlights, -100, 100);
        }
        else if (std::string("-shadows") == std::string(argv[i])) {
            shadows = atoi(next_arg(i).c_str());
            shadows = clamp<int>(shadows, -100, 100);
        }
        else if (std::string("-contrast") == std::string(argv[i])) {
            contrast = atoi(next_arg(i).c_str());
            contrast = clamp<int>(contrast, -100, 100);
        }
        else if (std::string("-brightness") == std::string(argv[i])) {
            brightness = atoi(next_arg(i).c_str());
            brightness = clamp<int>(brightness, -100, 100);
        }
        else if (std::string("-blackpoint") == std::string(argv[i])) {
            blackpoint = atoi(next_arg(i).c_str());
            blackpoint = clamp<int>(blackpoint, -100, 100);
        }
        else if (std::string("-saturation") == std::string(argv[i])) {
            saturation = atoi(next_arg(i).c_str());
            saturation = clamp<int>(saturation, -100, 100);
        }
        else if (std::string("-vibrance") == std::string(argv[i])) {
            vibrance = atoi(next_arg(i).c_str());
            vibrance = clamp<int>(vibrance, -100, 100);
        }
        else if (std::string("-warmth") == std::string(argv[i])) {
            warmth = atoi(next_arg(i).c_str());
            warmth = clamp<int>(warmth, -100, 100);
        }
        else if (std::string("-tint") == std::string(argv[i])) {
            tint = atoi(next_arg(i).c_str());
            tint = clamp<int>(tint, -100, 100);
        }
        else if (std::string("-definition") == std::string(argv[i])) {
            definition = atoi(next_arg(i).c_str());
            definition = clamp<int>(definition, 0, 100);  // definition range is [0, 100]
        }
        // --debug
        // Switch the SDK log level to VERBOSE for detailed diagnostic output.
        else if (std::string("--debug") == std::string(argv[i])) {
            ins::SetLogLevel(ins::InsLogLevel::VERBOSE);
        }
        // --log_level <verbose|info|warning|error|fatal>
        // Explicitly pick the SDK log level (case-insensitive). Overrides
        // --debug if it appears later on the command line; unrecognized
        // values are ignored with a warning.
        else if (std::string("--log_level") == std::string(argv[i])) {
            std::string level = next_arg(i);
            std::transform(level.begin(), level.end(), level.begin(), ::tolower);
            if (level == "verbose") {
                ins::SetLogLevel(ins::InsLogLevel::VERBOSE);
            } else if (level == "info") {
                ins::SetLogLevel(ins::InsLogLevel::INFO);
            } else if (level == "warning") {
                ins::SetLogLevel(ins::InsLogLevel::WARNING);
            } else if (level == "error") {
                ins::SetLogLevel(ins::InsLogLevel::ERR);
            } else if (level == "fatal") {
                ins::SetLogLevel(ins::InsLogLevel::FATAL);
            } else {
                std::cout << "[WARN] unknown --log_level value: " << level
                          << " (expected verbose|info|warning|error|fatal)" << std::endl;
            }
        }
        // --log_file [file]
        // Additionally duplicate the SDK log to a file (append mode, does not
        // affect stdout).  Value is optional: if a path follows, use it; if not
        // (next token is another -xxx option, or end of command line), fall back
        // to the default <exe>/logs/MediaSDKTest_<timestamp>.log.
        else if (std::string("--log_file") == std::string(argv[i])) {
            enable_log_file = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                log_path = std::string(argv[++i]);
            }
        }
        // -help
        // Print the parameter reference table and exit.
        else if (std::string("-help") == std::string(argv[i])) {
            std::cout << helpstr << std::endl;
        }
    }

    // Validation: at least one input file must be provided.
    if (input_paths.empty()) {
        std::cout << "can not find input_file" << std::endl;
        std::cout << helpstr << std::endl;
        return -1;
    }

    // Validation: either a single output file path (-output) or an image
    // sequence directory (-image_sequence_dir) must be specified.
    if (output_path.empty() && image_sequence_dir.empty()) {
        std::cout << "can not find output_file" << std::endl;
        std::cout << helpstr << std::endl;
        return -1;
    }

    // Parse the frame-index export sequence (e.g. "20-50-30" → {20, 50, 30}).
    // Only used when -image_sequence_dir is set together with -export_frame_index.
    std::vector<uint64_t> export_frame_nums;
    if (!image_sequence_dir.empty()) {
        auto frame_index_vec = split(exported_frame_number_sequence, '-');
        for (auto& frame_index : frame_index_vec) {
            int index = atoi(frame_index.c_str());
            export_frame_nums.push_back(index);
        }
    }

    // Inform the SDK where to find model files.
    // SetModelFileRootDir() requires the path to end with a path separator.
    ins::SetModelFileRootDir(model_root_dir);

    // --log_file: duplicate the SDK log to a file (append mode).  The SDK's
    // DupLogToFile does not create parent directories, so ensure the containing
    // folder exists first; otherwise the fopen inside the SDK silently fails.
    // Only write a file when --log_file was given (enable_log_file); otherwise
    // log to stdout only.  Value semantics (directory/file adaptive):
    //   - empty (bare --log_file with no value) → <exe>/logs/ directory
    //   - a directory (existing dir, or trailing '/' or '\') → create a
    //     timestamped file inside it
    //   - otherwise treat as a full file path
    // The directory check matters because users often pass a directory
    // (e.g. --log_file /path/logs); fopen on a directory fails and the log is
    // silently lost (the earlier "log not saved" issue seen on Linux).
    if (enable_log_file) {
        auto timestamped_name = []() -> std::string {
            std::time_t now = std::time(nullptr);
            std::tm tm_buf{};
    #ifdef _WIN32
            localtime_s(&tm_buf, &now);
    #else
            localtime_r(&now, &tm_buf);
    #endif
            std::ostringstream oss;
            oss << "MediaSDKTest_" << std::put_time(&tm_buf, "%Y%m%d_%H%M%S") << ".log";
            return oss.str();
        };

        std::error_code ec;
        std::filesystem::path fs_log_path;

        if (log_path.empty()) {
            fs_log_path = std::filesystem::u8path(getExecutablePath()) / "logs";
            std::filesystem::create_directories(fs_log_path, ec);
            fs_log_path /= timestamped_name();
        } else {
            fs_log_path = std::filesystem::u8path(log_path);
            bool as_dir = false;

            if (std::filesystem::is_directory(fs_log_path, ec)) {
                as_dir = true;
            } else if (!fs_log_path.has_filename() || fs_log_path.filename().empty()) {
                // 路径以 / 结尾 → 视为目录
                as_dir = true;
            }

            if (as_dir) {
                std::filesystem::create_directories(fs_log_path, ec);
                fs_log_path /= timestamped_name();
            } else {
                auto parent = fs_log_path.parent_path();
                if (!parent.empty()) {
                    std::filesystem::create_directories(parent, ec);
                }
            }
        }

        std::string final_log_path = fs_log_path.u8string();
        ins::SetLogPath(final_log_path);
        std::cout << "SDK log will also be written to: " << final_log_path << std::endl;
    }

    int count = 1;
    while (count--) {
        std::mutex mutex;
        std::condition_variable cond;
        bool is_finished = false;
        bool has_error = false;
        int stitch_progress = 0;

        // Dispatch to ImageStitcher or VideoStitcher based on the input file
        // extension.  Supported image extensions: .insp, .jpg.
        // Supported video extensions: .mp4, .insv, .lrv.
        std::string suffix = input_paths[0].substr(input_paths[0].find_last_of(".") + 1);
        std::transform(suffix.begin(), suffix.end(), suffix.begin(), ::tolower);
        if (suffix == "insp" || suffix == "jpg") {
            // ---- Image stitching path: synchronous — Stitch() blocks until done ----
            auto image_stitcher = std::make_shared<ImageStitcher>();

            // Input / output configuration
            image_stitcher->SetInputPath(input_paths);
            image_stitcher->SetStitchType(stitch_type);
            image_stitcher->SetOutputPath(output_path);
            image_stitcher->SetOutputSize(output_width, output_height);

            // Feature toggles
            image_stitcher->EnableFlowState(enable_flowstate);
            image_stitcher->EnableDenoise(enable_sequence_denoise);
            image_stitcher->EnableCuda(enable_cuda);
            image_stitcher->EnableStitchFusion(enalbe_stitchfusion);
            image_stitcher->SetCameraAccessoryType(accessory_type);
            image_stitcher->EnableColorPlus(enable_colorplus);
            image_stitcher->SetImageProcessingAccelType(image_processing_accel);
            image_stitcher->EnableCoolingShellDetection(enable_cooling_shell_detection);

            // Color / ISP adjustment parameters
            image_stitcher->SetTint(tint);
            image_stitcher->SetWarmth(warmth);
            image_stitcher->SetShadows(shadows);
            image_stitcher->SetContrast(contrast);
            image_stitcher->SetExposure(exposure);
            image_stitcher->SetVibrance(vibrance);
            image_stitcher->SetHighlights(highlights);
            image_stitcher->SetDefinition(definition);
            image_stitcher->SetBrightness(brightness);
            image_stitcher->SetBlackpoint(blackpoint);
            image_stitcher->SetSaturation(saturation);

            // Synchronous call — returns when stitching is complete.
            image_stitcher->Stitch();
        }
        else if (suffix == "mp4" || suffix == "insv" || suffix == "lrv") {
            // ---- Video stitching path: StartStitch() is asynchronous; progress
            //      and completion are reported via callbacks registered below ----
            auto start_time = steady_clock::now();
            auto video_stitcher = std::make_shared<VideoStitcher>();

            // Input configuration
            video_stitcher->SetInputPath(input_paths);

            if (image_sequence_dir.empty()) {
                // Standard output: stitch to a single video file.
                video_stitcher->SetOutputPath(output_path);
            }
            else {
                // Image-sequence output: export selected frames (or all frames
                // if no specific indices were given) as jpg/png files.
                if (!export_frame_nums.empty()) {
                    video_stitcher->SetExportFrameSequence(export_frame_nums);
                }

                video_stitcher->SetImageSequenceInfo(image_sequence_dir, image_type);
            }

            // Stitching algorithm and GPU settings
            video_stitcher->SetStitchType(stitch_type);
            video_stitcher->EnableCuda(enable_cuda);
            video_stitcher->EnableStitchFusion(enalbe_stitchfusion);

            // Output format settings
            video_stitcher->EnableColorPlus(enable_colorplus);
            video_stitcher->SetOutputSize(output_width, output_height);
            video_stitcher->SetOutputBitRate(output_bitrate);

            // Stabilization and orientation settings
            video_stitcher->EnableFlowState(enable_flowstate);
            video_stitcher->EnableDenoise(enable_sequence_denoise);
            video_stitcher->EnableDirectionLock(enable_directionlock);

            // Camera / codec settings
            video_stitcher->SetCameraAccessoryType(accessory_type);
            video_stitcher->SetSoftwareCodecUsage(enable_soft_encode, enable_soft_decode);
            if (enable_H265_encoder) {
                video_stitcher->EnableH265Encoder(true);
            }

            if (enable_10bit) {
                video_stitcher->Enable10BitExport(true);
            }

            // Post-processing / quality settings
            video_stitcher->EnableDefringe(enable_defringe);
            video_stitcher->EnableDeflicker(enable_deflicker);
            video_stitcher->EnableCoolingShellDetection(enable_cooling_shell_detection);
            video_stitcher->SetImageProcessingAccelType(image_processing_accel);

            // Color / ISP adjustment parameters
            video_stitcher->SetTint(tint);
            video_stitcher->SetWarmth(warmth);
            video_stitcher->SetShadows(shadows);
            video_stitcher->SetContrast(contrast);
            video_stitcher->SetExposure(exposure);
            video_stitcher->SetVibrance(vibrance);
            video_stitcher->SetHighlights(highlights);
            video_stitcher->SetDefinition(definition);
            video_stitcher->SetBrightness(brightness);
            video_stitcher->SetBlackpoint(blackpoint);
            video_stitcher->SetSaturation(saturation);

            // Progress callback: invoked by the SDK on its internal thread whenever
            // the stitching percentage changes.  'process' ranges from 0 to 100.
            // When 100 is reached the main thread is unblocked via the condition
            // variable.  The 'error' parameter is reserved (always 0 here; actual
            // errors come through the state callback below).
            video_stitcher->SetStitchProgressCallback([&](int process, int error) {
                if (stitch_progress != process) {
                    const std::string process_desc = "process = " + std::to_string(process) + std::string("%");
                    std::cout << "\r" << process_desc << std::flush;
                    stitch_progress = process;
                }

                if (stitch_progress == 100) {
                    std::cout << std::endl;
                    // Update shared state under the lock, then release and notify,
                    // so the waiting thread does not wake only to block again on a
                    // still-held mutex.
                    {
                        std::unique_lock<std::mutex> lck(mutex);
                        is_finished = true;
                    }
                    cond.notify_one();
                }
            });

            // Error / state callback: the SDK does not throw C++ exceptions; all
            // runtime errors are delivered here.  'error' is the numeric error code
            // (see SDKErrorCode in ins_common.h); 'err_info' is a human-readable
            // description.  Setting has_error and notifying the condition variable
            // causes the main thread to wake up and exit the wait loop.
            video_stitcher->SetStitchStateCallback([&](int error, const char* err_info) {
                std::cout << "error: " << err_info << std::endl;
                // Same as the progress callback: update the shared flag under the
                // lock before notifying, so the waiting thread is guaranteed to see
                // has_error already set when it wakes.
                {
                    std::unique_lock<std::mutex> lck(mutex);
                    has_error = true;
                }
                cond.notify_one();
            });

            std::cout << "start stitch " << std::endl;

            // Kick off stitching asynchronously.  The call returns immediately;
            // the actual work runs on SDK-managed threads.
            video_stitcher->StartStitch();

            // Block the main thread until the progress callback signals completion
            // (is_finished == true) or the state callback signals an error
            // (has_error == true).
            std::unique_lock<std::mutex> lck(mutex);
            cond.wait(lck, [&] {
                std::cout << "progress: " << video_stitcher->GetStitchProgress() << "; finished: " << is_finished << std::endl;
                return is_finished || has_error;
            });

            std::cout << "end stitch " << std::endl;

            auto end_time = steady_clock::now();
            std::cout << "cost = " << duration_cast<duration<double>>(end_time - start_time).count() << std::endl;

            // Runtime feature status map
            auto feature_map = video_stitcher->GetFeatureStatusMap();
            if (!feature_map.empty()) {
                std::cout << "--- Feature Status ---" << std::endl;
                for (auto& [k, v] : feature_map) {
                    std::string status;
                    switch (v) {
                        case -1: status = "UNKNOWN"; break;
                        case 0:  status = "OFF"; break;
                        case 1:  status = "ON"; break;
                        case 2:  status = "SKIP(auto)"; break;
                        case 3:  status = "FAILED"; break;
                        default: status = std::to_string(v); break;
                    }
                    std::cout << "  " << k << ": " << status << std::endl;
                }
                std::cout << "---" << std::endl;
            }
        }
    }
    return 0;
}
