#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>

#if WIN32
#ifdef USE_EXPORTDLL
#define CAMERASDK_API _declspec(dllexport)
#else
#define CAMERASDK_API _declspec(dllimport)
#endif
#else
#define CAMERASDK_API
#endif

namespace ins_camera {
    enum class CameraType {
        Insta360OneX,
        Insta360OneR,
        Insta360OneRS,
        Insta360OneX2,
        Insta360X3,
        Insta360X4,
        Insta360X5,
        Insta360X4Air,
        Insta360X6,
        Unknown,
    };

    enum class ConnectionType {
        USB,
        Wifi,
        Bluetooth
    };

    enum class VideoEncodeType {
        H264,
        H265
    };

    enum class CameraLensType {
        PanoDefault,
        Wide577,
        Pano577,
        Wide283,
        Pano283,
        Wide586,
        Pano586,
        Action577,
    };

    struct DeviceConnectionInfo {
        ConnectionType connection_type;
        std::string camera_name;
        void* native_connection_info;
    };

    struct DeviceDescriptor {
        CameraType camera_type;
        std::string serial_number;
        std::string camera_name;
        std::string fw_version;
        DeviceConnectionInfo info;
    };

    // WindowCropInfo is defined internally — external code accesses it
    // via PreviewParam getters only.
    struct WindowCropInfo;
    class CameraImpl;

    class CAMERASDK_API PreviewParam {
    public:
        PreviewParam();
        ~PreviewParam();
        PreviewParam(PreviewParam&&) noexcept;
        PreviewParam& operator=(PreviewParam&&) noexcept;

        std::string camera_name;
        VideoEncodeType encode_type;
        int64_t delay_timestamp{0};
        int64_t sweep_time{0};
        int32_t acceleration_range{0};
        int32_t gyro_range{0};

        // ── Calibration (opaque) ────────────────────────────────────────────
        size_t GetCalibrationCount() const;
        std::string GetCalibration(size_t index) const;

        // ── Crop info (opaque) ──────────────────────────────────────────────
        uint32_t GetCropSrcWidth() const;
        uint32_t GetCropSrcHeight() const;
        uint32_t GetCropDstWidth() const;
        uint32_t GetCropDstHeight() const;
        int32_t  GetCropOffsetX() const;
        int32_t  GetCropOffsetY() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
        friend class CameraImpl;
    };

    enum SensorDevice {
        SENSOR_DEVICE_FRONT = 1,
        SENSOR_DEVICE_REAR = 2,
        SENSOR_DEVICE_ALL = 3
    };

    enum CardState {
        STOR_CS_PASS = 0,
        STOR_CS_NOCARD = 1,
        STOR_CS_NOSPACE = 2,
        STOR_CS_INVALID_FORMAT = 3,
        STOR_CS_WPCARD = 4,
        STOR_CS_OTHER_ERROR = 5
    };

    enum class CardLocation {
        STOR_CL_CAMERA = 0,
        STOR_CL_READER = 1,
        STOR_CL_CAMERA_INNER = 2,  // Camera internal storage
        STOR_CL_CAMERA_SD = 3,     // Camera SD card
        STOR_CL_AUTO = 99,         // Auto-select (for legacy protocol compatibility)
    };

    struct StorageStatus {
        CardState state;
        uint64_t free_space;
        uint64_t total_space;
        CardLocation location = CardLocation::STOR_CL_CAMERA_SD;
    };

    enum LogLevel {
        VERBOSE = 0,
        INFO,
        WARNING,
        ERR,
        FATAL,
    };

    enum class SharpnessLevel {
        Low = 0x00,
        Medium = 0x01,
        High = 0x02,
    };

    enum class CaptureStoppedErrorCode {
        OVER_TIME_LIMIT = 0,
        STORAGE_FULL = 1,
        OTHER_SITUATION = 2,
        OVER_FILE_NUMBER_LIMIT = 3,
        LOW_CARD_SPEED = 4,
        MUXER_STREAM_ERROR = 5,
        DROP_FRAMES = 6,
        LOW_BATTERY = 7,
        STORAGEFRGMT = 8,
        HIGH_TEMP = 9,
        LOW_POWER_START = 10,
        STORAGE_RUNOUT_START = 11,
        HIGH_TEMP_START = 12,
        TASK_CONFLICT_START = 13,
        FW_UPDATE = 14
    };

    // RAW/PureShot capture type. Values match RawCaptureType in the proto (extra_info.proto / photo.proto).
    // Early models (e.g. X3) only offer a DNG on/off choice; later models support PureShot,
    // so TakePhoto now takes the type as a parameter.
    enum class RawCaptureType {
        Off = 0,         // Normal photo, without RAW
        DNG = 1,         // DNG RAW
        RAW = 2,         // Not yet supported
        PureShot = 3,    // PureShot
        PureShotRaw = 4, // PureShot + RAW
    };

    using DownloadProgressCallBack = std::function<void(int64_t download_size, int64_t total_size)>;
    using UploadProgressCallBack = std::function<void(int64_t total_size, int64_t upload_size)>;

    using BatteryLowCallBack = std::function<void(int battery_level)>;
    using StorageFullCallBack = std::function<void()>;
    using CaptureStoppedCallBack = std::function<void(const std::string& url, int err_code)>;
    using TemperatureHighCallBack = std::function<void()>;
    using CaptureStateCallBack = std::function<void(bool is_capture)>;

    class CAMERASDK_API MediaUrl {
    public:
        MediaUrl(const std::vector<std::string>& uris, const std::vector<std::string>& lrv_uris = std::vector<std::string>());
        bool Empty() const;
        bool IsSingleOrigin() const;
        bool IsSingleLRV() const;
        std::string GetSingleOrigin() const;
        std::string GetSingleLRV() const;
        const std::vector<std::string>& OriginUrls() const;
        const std::vector<std::string>& LRVUrls() const;
    private:
        std::vector<std::string> uris_;
        std::vector<std::string> lrv_uris_;
    };
}
