#pragma once

#include "../stream/stream_delegate.h"
#include "ins_types.h"
#include "photography_settings.h"
#include <memory>
#include <string>
#include <vector>
namespace ins_camera {
class CameraImpl;
/**
 * \class Camera
 * \brief A class that represents Insta360 camera, supports ONE X, ONE R, ONE RS, ONE X2, X3, X4, X5
 */
// set path of the log
void CAMERASDK_API SetLogPath(const std::string& log_path);

// set log level
void CAMERASDK_API SetLogLevel(LogLevel level);

// get SDK version
std::string CAMERASDK_API GetSDKVersion();

// get SDK major version
int CAMERASDK_API GetSDKVersionMajor();

class CAMERASDK_API Camera {
public:
    /**
     * \brief see also DeviceDiscovery to get DeviceConnectionInfo.
     */
    Camera(const DeviceConnectionInfo& info);
    /**
     * \brief Open camera and start session
     * \return true if succeed, otherwise return false;
     */
    bool Open() const;
    /**
     * \brief Close camera and release resources
     */
    void Close() const;

    /**
     * \brief get capture current status of camera.
     * be sure to call this function after Open()
     * \return lens type of the camera
     */
    bool CaptureCurrentStatus() const;

    /**
     * \brief set service port of download service
     * be sure to call this function before Open()
     */
    void SetServicePort(int port);

    /**
     * \brief Control camera to take normal photo
     * \param raw_type RAW/PureShot capture type. Defaults to RawCaptureType::PureShot.
     *      X4/X5/X6 support PureShot / PureShot+RAW; older models (e.g. X3) only support Off/DNG.
     * \param timeout_ms Max time to wait for the capture to finish, in milliseconds.
     *      0 (default) lets the SDK derive a timeout from the current shutter speed and self-timer
     *      (long exposures / countdown get more time automatically). Pass a positive value to force
     *      a specific timeout (e.g. a shorter one for fast snapshots).
     * \return The url of the photo if success, otherwise empty
     */
    MediaUrl TakePhoto(RawCaptureType raw_type = RawCaptureType::PureShot, int timeout_ms = 0) const;

    /**
     * \brief Control camera to take hdr photo.
     *
     * ## HDR/AEB photo setup flow (X4Air/X5/X6):
     * 1. SetPhotoHdrMode(mode, PHOTO_HDR_OFF/PHOTO_HDR_AUTO/PHOTO_HDR_AEB) — choose HDR mode
     * 2. If AEB mode:
     *    - SetAebCaptureNum(mode, 3/5/7) — bracket count (check capability table for valid values)
     *    - SetAebExposureBias(mode, 0.3~4.0) — EV step per bracket (check capability table)
     * 3. StartHDRCapture(photo_size, ...) — take the photo
     *
     * ## Encoding notes:
     * - ev_step_tenths: 0.3 EV → 3, 1.0 EV → 10, 2.0 EV → 20
     * - aeb_num: pass 0 to keep camera's current value (same for ev_step_tenths)
     * - When both aeb_num and ev_step_tenths are > 0, bracketing offsets are auto-generated
     *
     * ## Capability table:
     * Use GetSupportedAttrValues(mode, "hdr_photo_mode") / "aeb_capture_num" / "exposure_bias"
     * to query valid values for the current camera. Actual values depend on camera model and
     * current photo resolution (e.g. X6 120MP only supports AEB 3/5, not 7).
     *
     * @code
     * // Example: AEB photo with 5 shots, 1.0 EV step
     * auto fmode = CameraFunctionMode::FUNCTION_MODE_NORMAL_IMAGE;
     * cam->SetPhotoHdrMode(fmode, PhotoHdrType::PHOTO_HDR_AEB);
     * cam->SetAebCaptureNum(fmode, 5);
     * cam->SetAebExposureBias(fmode, 1.0);
     * auto url = cam->StartHDRCapture(photo_size, false, 5, 10);
     * @endcode
     *
     * \param photo_size photo size (use GetSupportedPhotoSizes to get valid values)
     * \param is_raw true=PureShot+RAW / AEB bracket; false=AUTO HDR
     * \param aeb_num  Number of AEB bracketed shots (3/5/7). 0 = keep camera's current value.
     * \param ev_step_tenths AEB exposure step per stop in units of 0.1 EV. 0 = keep current.
     * \param raw_type RAW/PureShot capture type. Defaults to RawCaptureType::PureShot.
     * \return The url of the photo if success, otherwise empty
     */
    MediaUrl StartHDRCapture(const PhotoSize& photo_size, bool is_raw = false, int aeb_num = 0, int ev_step_tenths = 0,
                             RawCaptureType raw_type = RawCaptureType::PureShot);

    /**
     * \brief set exposure settings, the settings will only be applied to specified mode
     * \param mode the target mode you want to apply exposure settings, for normal video recording,
     *      use CameraFunctionMode::FUNCTION_MODE_NORMAL_VIDEO, for normal still image capture,
     *      use CaperaFunctionMode::FUNCTION_MODE_NORMAL_IMAGE.
     * \param settings ExposureSettings containing exposure mode/iso/shutter/ev to be applied.
     * \return true on success, false otherwise.
     */
    bool SetExposureSettings(CameraFunctionMode mode, const std::shared_ptr<ExposureSettings>& settings);

    std::shared_ptr<ExposureSettings> GetExposureSettings(CameraFunctionMode mode) const;

    /**
     * \brief set capture settings, the settings will only be applied to specified mode
     * \param mode the target mode you want to apply exposure settings, for normal video recording,
     *      use CameraFunctionMode::FUNCTION_MODE_NORMAL_VIDEO, for normal still image capture,
     *      use CaperaFunctionMode::FUNCTION_MODE_NORMAL_IMAGE.
     * \param settings CaptureSettings containing capture settings like
     * saturation,contrast,whitebalance,sharpness,brightness and etc.
     * \return true on success, false otherwise.
     */
    bool SetCaptureSettings(CameraFunctionMode mode, std::shared_ptr<CaptureSettings> settings);

    std::shared_ptr<CaptureSettings> GetCaptureSettings(CameraFunctionMode mode) const;

    /**
     * \brief Force re-sync photography options from the camera for the given mode.
     *        Normally GetCaptureSettings / GetExposureSettings read from a cache;
     *        call this after SetXXX to confirm the camera's real current values.
     * \return true on success, false if the camera did not respond.
     */
    bool SyncPhotographyOptions(CameraFunctionMode mode);

    /**
     * \brief set capture settings such as resolutions, bitrate,
     * \param params RecordParams containing settings you want to apply
     * \param mode the target mode you want to apply capture settings, the mode must be one of video modes.
     * \return true on success, false otherwise.
     */
    bool SetVideoCaptureParams(RecordParams params,
                               CameraFunctionMode mode = CameraFunctionMode::FUNCTION_MODE_NORMAL_VIDEO);

    /**
     * \brief Start Recording.
     * \return true on success, false otherwise
     */
    bool StartRecording();

    /**
     * \brief Stop Recording.
     * \return video uri of the video, it may contain low bitrate proxy video
     */
    MediaUrl StopRecording();

    /**
     * \brief start preview stream
     */
    bool StartLiveStreaming(const LiveStreamParam& param);

    /**
     * \brief stop preview stream
     */
    bool StopLiveStreaming();

    /**
        \brief set a stream delegate, you may implement StreamDelegate to handle stream data.
     */
    void SetStreamDelegate(std::shared_ptr<StreamDelegate>& delegate);

    /**
     * \brief Delete the specified file from camera
     */
    bool DeleteCameraFile(const std::string& filePath) const;

    /**
     * \brief Download the specified file from camera
     */
    bool DownloadCameraFile(const std::string& remoteFilePath, const std::string& localFilePath,
                            DownloadProgressCallBack callback = nullptr) const;

    /**
     * \brief upload Firmware to camera. only support X4
     */
    bool UploadFile(const std::string& localFilePath, const std::string& remoteFilePath,
                    UploadProgressCallBack callback = nullptr) const;

    /**
     * \brief cancal the file that downloading, and it will not be deleted it
     * need delete it by caller.
     */
    void CancelDownload();

    /**
     * \brief Get the list of files stored in camera storage card
     */
    std::vector<std::string> GetCameraFilesList() const;

    /**
     * \brief Get the count of files stored in camera storage card
     */
    bool GetCameraFilesCount(int& count) const;

    /**
     * \brief set timelapse param
     */
    bool SetTimeLapseOption(TimelapseParam params);

    /**
     * \brief start timelapse
     */
    bool StartTimeLapse(CameraTimelapseMode mode);

    /**
     * \brief stop timelapse
     * \return video uri of the video, it may contain low bitrate proxy video
     */
    MediaUrl StopTimeLapse(CameraTimelapseMode mode);

    /**
     * \brief sync local time to camera
     * \param utc_time current UTC timestamp in seconds (e.g. time(nullptr))
     * \param offset_time time-zone offset from UTC in seconds, positive = east of
     *   GMT, negative = west (e.g. +28800 for UTC+8, -18000 for UTC-5)
     */
    bool SyncLocalTimeToCamera(uint64_t utc_time, int32_t offset_time);

    /**
     * \brief device is connected
     */
    bool IsConnected();

    /**
     * \brief SetTimeout, default is 10000ms
     */
    void SetTimeout(int time_out_ms);

    /**
     * \brief get battry info
     */
    bool GetBatteryStatus(BatteryStatus& status);

    /**
     * \brief get storage state for the specified storage location (X6+).
     *        For older cameras, falls back to the default storage.
     * \param status output storage status
     * \param location target storage location (inner or SD card, auto)
     * \return true on success
     */
    bool GetStorageState(StorageStatus& status, CardLocation location = CardLocation::STOR_CL_AUTO);

    /**
     * \brief Format the specified storage in camera (X6+).
     *        For older cameras, falls back to FormatSDCard().
     * \param location target storage location to format (inner or SD card, auto)
     * \return true if the format command was acknowledged by the camera, false on timeout or failure.
     */
    bool FormatStorage(CardLocation location = CardLocation::STOR_CL_AUTO) const;

    /**
     * \brief Switch the camera's main storage location (X6+).
     *        Older cameras only support SD card.
     * \param location target storage location
     * \return true on success
     */
    bool SetStorageLocation(CardLocation location);

    /**
     * \brief Get the current storage location of the camera.
     * \return current CardLocation
     */
    CardLocation GetStorageLocation() const;

    /**
     * \brief set photo size on photo mode
     */
    bool SetPhotoSize(CameraFunctionMode mode, const PhotoSize& photo_size);

    /**
     * \brief Set the HDR photo mode for the given function mode.
     *        X4Air/X5/X6: HDR is a photography option on PHOTO_SINGLE, not a separate mode.
     *        X4 and earlier: HDR is a separate PHOTO_HDR function mode.
     * \param mode target function mode (use FUNCTION_MODE_NORMAL_IMAGE for HDR photo)
     * \param hdr_type PHOTO_HDR_OFF (0) / PHOTO_HDR_AUTO (1) / PHOTO_HDR_AEB (2)
     * \return true on success
     */
    bool SetPhotoHdrMode(CameraFunctionMode mode, PhotoHdrType hdr_type);
    /**
     * \brief Set the AEB (auto exposure bracketing) shot count.
     *        Valid values depend on camera model and current photo resolution.
     *        Query capability table: GetSupportedAttrValues(mode, "aeb_capture_num")
     *        Typical values: 3, 5, 7 (X6 120MP max is 5).
     * \param mode target function mode
     * \param num bracket count (3/5/7)
     * \return true on success
     */
    bool SetAebCaptureNum(CameraFunctionMode mode, int num);
    /**
     * \brief Set the AEB exposure step per bracket, in EV units.
     *        Valid values depend on camera model and AEB capture count.
     *        Query capability table: GetSupportedAttrValues(mode, "exposure_bias")
     *        Typical values: 0.3, 0.6, 1.0, 1.3, 1.6, 2.0, 2.3, 2.6, 3.0, 3.3, 3.6, 4.0
     *        (X5 with 7-shot AEB is limited to ±0.3 ~ ±1.3).
     * \param mode target function mode
     * \param ev exposure step in EV (e.g. 1.0 means -1/0/+1 EV, 2.0 means -2/0/+2 EV)
     * \return true on success
     */
    bool SetAebExposureBias(CameraFunctionMode mode, double ev);

    /** \brief Set the photo capture format (PureShot / PureShot+RAW, etc.) for the given mode.
     *         For X5/X6 this must be preset via this call; the raw_type passed to TakePhoto alone
     *         may be ignored by the camera. */
    bool SetRawCaptureType(CameraFunctionMode mode, RawCaptureType raw_type);

    /**
     * \brief Look up the integer value of a capability-table string via the proto map.
     *        E.g. GetAttrValueByName("raw_capture_type", "PURESHOT") -> 3 (RawCaptureType::PureShot).
     *        Returns -1 if the attr or value name is unknown.
     */
    int GetAttrValueByName(const std::string& attr_name, const std::string& value_name);


    /**
     * \brief Get the photo sizes supported by the current camera in the given mode.
     *        Reads the capability table fetched at connect time (no network request).
     *        Returns empty if the capability table is unavailable or the mode has none.
     */
    std::vector<PhotoSize> GetSupportedPhotoSizes(CameraFunctionMode mode) const;

    /**
     * \brief Get the photo modes supported by the current camera.
     *        Reads the capability table fetched at connect time (no network request).
     *        Returns all PHOTO_* modes that exist in the capability table.
     *        Returns empty if the capability table is unavailable.
     */
    std::vector<CameraFunctionMode> GetSupportedPhotoModes() const;

    /**
     * \brief Get the video modes supported by the current camera.
     *        Reads the capability table fetched at connect time (no network request).
     *        Returns all VIDEO_* modes that exist in the capability table.
     *        Returns empty if the capability table is unavailable.
     */
    std::vector<CameraFunctionMode> GetSupportedVideoModes() const;

    /**
     * \brief Get the attribute names supported by the current camera in the given mode
     *        (e.g. "record_resolution", "lapse_time", "exposure_iso").
     *        Reads the capability table fetched at connect time (no network request).
     *        Returns empty if the capability table is unavailable or the mode has none.
     */
    std::vector<std::string> GetSupportedAttrNames(CameraFunctionMode mode) const;
    /**
     * \brief Get the supported values of an attribute in the given mode (as strings;
     *        caller parses int/double/enum/bool as needed).
     *        depend_on_context empty  -> the "default" branch.
     *        depend_on_context non-empty -> the branch keyed by it (e.g. lapse_time with
     *        "7680_3840_30|custom" returns the 8K@30 + custom-scene interval options).
     *        The context key is built by joining the selected values of the attributes
     *        returned by GetAttrDependOn, in their order, with '|'.
     *        Returns empty if the capability table is unavailable or the branch is absent.
     */
    std::vector<std::string> GetSupportedAttrValues(CameraFunctionMode mode, const std::string& attr,
                                                    const std::string& depend_on_context = "") const;
    /**
     * \brief Get the depend_on descriptor of an attribute: which attributes' selected
     *        values affect this attribute's options, and the order to join them into
     *        depend_on_context. Returns empty if the attribute has no depend_on.
     */
    std::vector<std::string> GetAttrDependOn(CameraFunctionMode mode, const std::string& attr) const;
    /**
     * \brief Get the canonical name of a VideoResolution (e.g. "7680_3840_50"),
     *        via the record_resolution proto reverse map. Returns empty if unknown.
     *        Used by demos to parse the "resolution -> framerate" hierarchy.
     */
    std::string GetVideoResolutionName(VideoResolution res) const;

    /**
     * \brief Get the video resolutions supported by the current camera in the given mode.
     *        Reads the capability table fetched at connect time (no network request).
     *        Returns empty if the capability table is unavailable or the mode has none.
     */
    std::vector<VideoResolution> GetSupportedVideoResolutions(CameraFunctionMode mode) const;

    /**
     * \brief Get the accelerate-frequencies (timeshift/timelapse speed multipliers; 0=Auto)
     *        supported by the current camera in the given mode.
     *        Reads the capability table fetched at connect time (no network request).
     *        Returns empty if the capability table is unavailable or the mode has none.
     */
    std::vector<int> GetSupportedAccelerateFrequencies(CameraFunctionMode mode) const;

    /**
     * \brief switch sensor
     */
    bool SetActiveSensor(SensorDevice sensor_device);

    /**
     * \brief get media time from camera
     */
    int64_t GetCameraMediaTime() const;

    /**
     * \brief set sub mode on video mode
     */
    bool SetVideoSubMode(SubVideoMode video_mode);

    /**
     * \brief set sub mode on photo mode
     */
    bool SetPhotoSubMode(SubPhotoMode photo_mode);

    /**
     * \brief Obtain the information of the file being recorded
     */
    bool GetRecordingFiles(std::vector<std::string>& file_list);

    /**
     * \brief preview param of camera
     * be sure to call this function after Open()
     */

    PreviewParam GetPreviewParam() const;

    /**
     * \brief get current camera encode type(h264 or h265)
     */
    VideoEncodeType GetVideoEncodeType() const;

    CameraFunctionMode GetCurrentFunctionMode() const;

    /**
     * \brief enable In-camera stitching
     */
    bool EnableInCameraStitching(bool enable);

    /**
     * \brief enable auto stop record after camera disconnet
     */
    bool EnableAutoStopRecordAfterDisconnet(bool enable);

    /**
     * \brief shutdown camera
     */
    bool ShutdownCamera() const;

    bool SetGlobalSharpness(SharpnessLevel value);
    bool GetGlobalSharpness(SharpnessLevel& value);
    /**
     * \brief Get url of camera log
     */
    std::string GetCameraLogFileUrl() const;

    /**
     * \brief Set the notification callback for low battery
     */
    void SetBatteryLowNotification(BatteryLowCallBack callback);

    /**
     * \brief Set the notification callback for storage full
     */
    void SetStorageFullNotification(StorageFullCallBack callback);

    /**
     * \brief Set the notification callback for recording termination
     * \ param: error_code:  "CaptureStoppedErrorCode"
     */
    void SetCaptureStoppedNotification(CaptureStoppedCallBack callback);

    /**
     * \brief Set the notification callback for temperature high
     */
    void SetTemperatureHighNotification(TemperatureHighCallBack callback);

    void SetCaptureStateNotification(CaptureStateCallBack callback);

    CameraLensType GetCameraLensType() const;

    std::string GetHttpBaseUrl() const;

    bool SetPhotoSubModeTimer(SubPhotoMode photo_mode, int duration_sec);

    int GetSelfTimer(CameraFunctionMode mode) const;

private:
    std::shared_ptr<CameraImpl> impl_;
};
} // namespace ins_camera
