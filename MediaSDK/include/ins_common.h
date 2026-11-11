#pragma once
#include <functional>
#include <map>
#include <vector>
#include <string>
#include <memory>

#if WIN32
#ifdef USE_EXPORTDLL
#define MEDIASDK_API _declspec(dllexport)
#else
#define MEDIASDK_API _declspec(dllimport)
#endif
#else
#define MEDIASDK_API
#endif

namespace ins {
    /**
     * \brief Callback invoked periodically to report stitching progress.
     * \param process Current progress value in the range [0, 100].
     * \param error   Non-zero error code if an error occurred during processing, otherwise 0.
     */
    typedef std::function<void(int process, int error)> stitch_process_callback;

    /**
     * \brief Callback invoked when the stitcher encounters a fatal error or changes state.
     * \param error   SDK error code identifying the failure (see SDKErrorCode).
     * \param errinfo Human-readable description of the error, may be null.
     */
    typedef std::function<void(int error, const char* errinfo)> stitch_error_callback;

    /**
     * \brief Callback invoked for every stitched output frame during real-time stitching.
     * \param data      Array of up to 4 plane pointers for planar pixel formats (e.g. YUV).
     * \param linesize  Stride in bytes for each plane.
     * \param width     Width of the output frame in pixels.
     * \param height    Height of the output frame in pixels.
     * \param format    Pixel format identifier (FFmpeg AVPixelFormat convention).
     * \param timestamp Presentation timestamp of the frame in microseconds.
     */
    typedef std::function<void(uint8_t* data[4], int linesize[4], int width, int height, int format, int64_t timestamp)> stitch_realtime_data_callback;

    /**
     * \brief Stitching algorithm to use when combining lens images.
     */
    enum class STITCH_TYPE {
        TEMPLATE,       // Template-based stitching: fastest, uses pre-computed warp maps
        OPTFLOW,        // Optical-flow stitching: improves seam quality at moderate cost
        DYNAMICSTITCH,  // Dynamic optical-flow stitching: per-frame flow for moving scenes
        AIFLOW          // AI-based stitching: highest quality, slowest; requires model files
    };

    /**
     * \brief Output image format for image-sequence export.
     */
    enum class IMAGE_TYPE {
        JPEG,   // JPEG compressed image
        PNG,    // PNG lossless image
    };

    enum class ImageProcessingAccel { kAuto = 0, kCPU };

    /**
     * \brief Describes the physical accessory (lens guard, dive case, ND filter, etc.)
     *        mounted on the camera so that the stitcher can apply the correct correction profile.
     */
    enum class CameraAccessoryType {
        kAutoDetect = -1,       // Auto-detect the accessory from file metadata
        kNormal = 0,            // No accessory (bare camera)
        kWaterproof,            // Waterproof dive case (ONE / ONE X / ONE X2 / ONE R / ONE RS / ONE X3)
        kOnerLensGuard,         // Adhesive lens guard (ONE R / ONE RS)
        kOnerLensGuardPro,      // Snap-on lens guard pro (ONE R / ONE RS)
        kOnex2LensGuard,        // Adhesive lens guard (ONE R / ONE RS / ONE X2 / ONE X3)
        kOnex2LensGuardPro,     // Snap-on lens guard pro (ONE X2)
        k283PanoLensGuardPro,   // Snap-on lens guard pro for 283-degree panoramic lens (ONE R / ONE RS)
        kDiveCaseAir,           // Dive case used above water (ONE X / ONE X2 / ONE R / ONE RS / ONE X3)
        kDiveCaseWater,         // Dive case used under water (ONE X / ONE X2 / ONE R / ONE RS / ONE X3)
        kInvisibleDiveCaseAir,  // Invisible dive case above water (X3 / X4 / X5 / X4 AIR)
        kInvisibleDiveCaseWater,// Invisible dive case under water (X3 / X4 / X5 / X4 AIR)
        kLensGuardA,            // Grade-A plastic lens guard (X3 / X4 / X5 / X4 AIR)
        kLensGuardS,            // Grade-S glass lens guard (X3 / X4 / X5 / X4 AIR)
        kLensGuardAS,           // Auto-detect between grade-A and grade-S (X3 / X4)
        kOnex5ND16,             // ND16 filter (X5)
        kOnex5ND32,             // ND32 filter (X5)
        kOnex5ND64,             // ND64 filter (X5)
        kOner283LensGuardPro,   // Lens guard pro for 283-degree lens on ONE R / ONE RS
        kOnerLensGuardFpv,      // Adhesive lens guard for FPV lens (non-283 ONE R / ONE RS)
        kOnex4AirDiveCaseAir,   // Dive case above water (X4 AIR)
        kOnex4AirDiveCaseWater, // Dive case under water (X4 AIR)
        kUndetermined = 100,    // Accessory type could not be determined
    };

    /**
     * \brief Log verbosity levels. Only messages at or above the configured level are emitted.
     */
    enum class InsLogLevel {
        VERBOSE = 0,    // All messages including fine-grained trace output
        INFO,           // General informational messages
        WARNING,        // Recoverable issues that may affect output quality
        ERR,            // Errors that caused an operation to fail
        FATAL,          // Unrecoverable errors that will terminate the process
    };

    /**
     * \brief Error codes returned by SDK operations and delivered via stitch_error_callback.
     */
    enum class SDKErrorCode {
        E_SUCCESS = 0,                      // Operation completed successfully
        E_OPEN_FILE = 1,                    // Failed to open input file
        E_PARSE_METADATA = 2,               // Failed to parse file metadata / trailer
        E_CREATE_OFFSCREEN = 3,             // Failed to create offscreen render target
        E_CREATE_RENDER_MODEL = 4,          // Failed to create render model
        E_FRAME_PARSE = 5,                  // Failed to retrieve a video frame
        E_CREATE_RENDER_SOURCE = 6,         // Failed to create render data source
        E_UPDATE_RENDER_SOURCE = 7,         // Failed to upload a frame to the render source
        E_RENDER_FRAME = 8,                 // Failed to render a frame
        E_SAVE_FRAME = 9,                   // Failed to save output image
        E_VIDEO_FRAME_EXPORTOR = 10,        // Failed to create video frame exporter
        E_FILE_TYPE_UNSUPPORT = 11,         // Input file type is not supported
        E_INTERNAL_ERROR = 998,             // Internal SDK error; may indicate a bug
        E_UNKNOWN = 999                     // Unknown error
    };

    /**
     * \brief Media capture mode recorded in the file, used to select the appropriate processing pipeline.
     */
    enum class MediaFileType {
        UNDEFINED = 0xffff,
        VIDEO_NORMAL = 0,           // Standard video recording
        VIDEO_BULLETTIME = 1,       // Bullet-time (slow-motion 360) video
        VIDEO_TIMELAPSE = 2,        // Time-lapse video
        VIDEO_HDR = 6,              // HDR video
        VIDEO_STATIC_TIMELAPSE = 8, // Static time-lapse video
        VIDEO_TIMESHIFT = 9,        // Time-shift (moving time-lapse) video
        VIDEO_SUPER_NORMAL = 11,    // Super-normal (high-quality) video
        VIDEO_LOOPRECORDING = 12,   // Loop recording video
        VIDEO_FPV = 15,             // FPV (first-person view) video
        VIDEO_MOVIE = 16,           // Movie mode video
        VIDEO_SLOWMOTION = 17,      // Slow-motion video
        VIDEO_SELFIE = 18,          // Follow / selfie video
        VIDEO_PURE = 20,            // PureShot night video
        VIDEO_STARLAPSE = 21,       // Star-lapse video
        VIDEO_DASH_CAM = 23,        // Dash-cam recording
        VIDEO_VIR_PTZ = 24,         // Virtual PTZ video

        PHOTO_NORMAL = 3,           // Standard photo
        PHOTO_HDR = 4,              // HDR photo
        PHOTO_INTERVALSHOOTING = 5, // Interval / time-lapse photo
        PHOTO_BURST = 7,            // Burst photo
        PHOTO_AEB_NIGHT_MODE = 10,  // AEB night-mode photo
        PHOTO_STARLAPSE = 13,       // Star-lapse photo
        PHOTO_PANO_MODE = 14,       // Panoramic photo
        PHOTO_PURESHOTPLUS = 19,    // PureShot+ photo
        PHOTO_STARTRAIL = 22,       // Star-trail photo
    };

    /**
     * \brief Video codec used to encode or decode a stream.
     */
    enum class VideoDecodeType {
        kH264,  // H.264 / AVC codec
        kH265   // H.265 / HEVC codec
    };

    /**
     * \brief A single IMU sample from the camera's gyroscope and accelerometer.
     *        Used by HandleGyroData() to drive FlowState stabilisation.
     */
    struct GyroData {
        int64_t timestamp; ///< Sample timestamp in microseconds (camera clock).
        double ax;         ///< Accelerometer X-axis reading (m/s^2).
        double ay;         ///< Accelerometer Y-axis reading (m/s^2).
        double az;         ///< Accelerometer Z-axis reading (m/s^2).
        double gx;         ///< Gyroscope X-axis angular velocity (rad/s).
        double gy;         ///< Gyroscope Y-axis angular velocity (rad/s).
        double gz;         ///< Gyroscope Z-axis angular velocity (rad/s).
    };

    /**
     * \brief Exposure metadata for a single captured frame.
     *        Passed to HandleExposureData() to assist tone mapping and HDR processing.
     */
    struct ExposureData {
        double timestamp;     ///< Timestamp of the frame in microseconds (camera clock).
        double exposure_time; ///< Shutter / exposure duration in seconds.
    };

    /**
     * \brief Camera description required by RealTimeStitcher to configure the stitching pipeline.
     *        Provides the camera model name, calibration data and codec type.
     *
     *        Calibration data is opaque; use SetCalibration() to populate it.
     */
    class MEDIASDK_API CameraInfo {
    public:
        CameraInfo();
        ~CameraInfo();
        CameraInfo(const CameraInfo&);
        CameraInfo& operator=(const CameraInfo&);

        std::string cameraName;                  ///< Camera model identifier string (e.g. "ONE X2").
        VideoDecodeType decode_type{VideoDecodeType::kH264}; ///< Codec used to decode the incoming video stream.
        int64_t gyro_timestamp{0};               ///< Timestamp of the first gyro sample (microseconds), used for sync.
        int64_t sweep_timestamp{0};              ///< Sweep/scan timestamp used for rolling-shutter correction (microseconds).

        /// Set per-lens calibration offsets and crop geometry.
        /// The vector grows as needed; no fixed-size bound is exposed.
        void SetCalibration(const std::vector<std::string>& offsets,
                            uint32_t src_width, uint32_t src_height,
                            uint32_t dst_width, uint32_t dst_height,
                            int32_t crop_offset_x, int32_t crop_offset_y);

    private:
        struct Impl {
            std::vector<std::string> offsets;       // Dynamically sized; grows as needed
            uint32_t crop_src_width{};
            uint32_t crop_src_height{};
            uint32_t crop_dst_width{};
            uint32_t crop_dst_height{};
            int32_t  crop_offset_x{};
            int32_t  crop_offset_y{};
        };
        std::unique_ptr<Impl> impl_;
        friend class RealTimeStitcherImp;
    };

    /**
     * \brief Basic properties of an Insta360 media file returned by GetMediaFileInfo().
     */
    struct MediaFileInfo {
        MediaFileType media_type; ///< Capture mode / file category (see MediaFileType).
        int width;                ///< Frame width in pixels.
        int height;               ///< Frame height in pixels.
        double fps;               ///< Frame rate in frames per second.
        int64_t bitrate;          ///< Video bitrate in bits per second.
        int64_t duration_ms;      ///< Total media duration in milliseconds.
    };

    /// Returns the full SDK version string, e.g. "3.1.4" (major.minor.patch).
    std::string MEDIASDK_API GetVersion();
    /// Returns the SDK major version number as an integer.
    int MEDIASDK_API GetVersionMajor();
    // Log level convenience aliases
    constexpr auto LOG_VERB  = InsLogLevel::VERBOSE;
    constexpr auto LOG_INFO  = InsLogLevel::INFO;
    constexpr auto LOG_WARN  = InsLogLevel::WARNING;
    constexpr auto LOG_ERR   = InsLogLevel::ERR;
    constexpr auto LOG_FATAL = InsLogLevel::FATAL;
}

// SDK_LOG — 带 [SDK] 前缀的日志宏，直接复用 llog 的 LOG()
// 用法：SDK_LOG(ins::LOG_INFO, "text " << var);
#define SDK_LOG(level, ...) LOG((int)(level)) << "[SDK] " << __VA_ARGS__
