#pragma once

#include "ins_common.h"

#if WIN32
#ifdef USE_EXPORTDLL
#define MEDIASDK_API _declspec(dllexport)
#else
#define MEDIASDK_API _declspec(dllimport)
#endif // USE_EXPORTDLL
#else
#define MEDIASDK_API
#endif

namespace ins {
    class StitcherImp;
    /**
     * \class VideoStitcher
     * \brief A class that stitch video, only support the formats that maked by Insta360 camera, for example *.insv .
     *   If you want to make the most of multiple GPUs, you must create process instances to correspond to each GPU.
     *   In a process instance, multiple GPUs are not supported.
    */

    /**
     * \brief Initialize the SDK environment. Must be called once before using any other SDK function.
     */
    void MEDIASDK_API InitEnv();

    /**
     * \brief Set the directory path where SDK log files will be written.
     * \param log_path Absolute or relative path to the log output directory.
     */
    void MEDIASDK_API SetLogPath(const std::string& log_path);

    /**
     * \brief Set the minimum log level. Messages below this level will be suppressed.
     * \param level One of the InsLogLevel values (VERBOSE, INFO, WARNING, ERR, FATAL).
     */
    void MEDIASDK_API SetLogLevel(InsLogLevel level);

    class MEDIASDK_API RealTimeStitcher {
    public:
        /**
         * \brief Construct a RealTimeStitcher instance.
         *        Call SetCameraInfo() and SetStitchRealTimeDataCallback() before calling StartStitch().
         */
        RealTimeStitcher();

        /**
         * \brief Destructor. Stops any active real-time stitch session and releases all internal resources.
         */
        ~RealTimeStitcher();

    public:
        /**
            * \brief set CameraAccessoryType
        */
        void SetCameraAccessoryType(CameraAccessoryType type);

        /**
         * \brief set the type of stitch,
         * \param type  it has four kind that has template,optflow,dynamicStitch,AI-Stitching and
         * \            template is fastest and AI-Stitching is slowest.
         * \            When the stitch type is AI-Stitching, the AI-Stitching model file must be set.
         * \            You can refer to this interface 'SetAiStitchModelFile ',
         * \            otherwise Ai-Stitching will not take effect
         */
        void SetStitchType(STITCH_TYPE type);

        /**
         *  \brief set the callback that can tell the client the error if stitcher goes error
         *  \param callback used for get err info of the process.
         *  \param lcontent the context
         */
        void SetStitchStateCallback(stitch_error_callback callback);

        /**
         *  \brief Set the camera parameters required for real-time preview stitching.
         *  \param cameraInfo Camera description including name, crop window, lens offsets and decode type.
         */
        void SetCameraInfo(const CameraInfo& cameraInfo);

        /**
         *  \brief start stitch
         */
        void StartStitch();

        /**
         *  \brief cancel stitch
         */
        bool CancelStitch();

        /**
         * \brief Set the callback invoked for each stitched output frame during real-time stitching.
         * \param callback Function called with raw pixel planes, line sizes, dimensions, pixel format
         *                 and a timestamp (microseconds) for every rendered frame.
         */
        void SetStitchRealTimeDataCallback(stitch_realtime_data_callback callback);

        /**
        * \brief set the video resolution of the output file，default size of source
        */
        void SetOutputSize(int width, int height);

        /**
        * \brief Whether to turn on FlowState
        * \param enable true is on and false is off, default off
        */
        void EnableFlowState(bool enable);

        /**
         * \brief Whether to lock the horizon/direction during real-time stitching.
         *        Requires FlowState to be enabled first.
         * \param enable true to lock the output direction, false to disable.
         */
        void EnableDirectionLock(bool enable);

        /**
         * \brief Introduce an artificial delay on the video stream to allow gyro data to align.
         * \param video_delay_ms Delay in milliseconds to apply to incoming video frames.
         */
        void SetVideoDelayMs(int video_delay_ms);

        /**
        * \brief enable deflicker
        */
        void EnableDeflicker(bool enable);

        /**
        * \brief Enable or disable software (CPU-based) codec implementations.
        * \param enable_encoder true to use a software encoder, false to prefer hardware encoding.
        * \param enable_decoder true to use a software decoder, false to prefer hardware decoding.
        */
        void SetSoftwareCodecUsage(bool enable_encoder, bool enable_decoder);

        /**
        * \brief enable Defringe
        */
        void EnableDefringe(bool enable);

        /**
        * \brief Feed a compressed video data packet into the real-time stitcher.
        * \param data         Pointer to the encoded video buffer.
        * \param size         Byte length of the buffer.
        * \param timestamp    Presentation timestamp of the packet in microseconds.
        * \param stream_type  Codec type identifier for the stream.
        * \param stream_index Index of the lens/stream when multiple streams are present (default 0).
        */
        void HandleVideoData(const uint8_t* data, size_t size, int64_t timestamp, uint8_t stream_type, int stream_index = 0);

        /**
        * \brief Feed gyroscope/IMU samples into the real-time stitcher for stabilisation.
        * \param data Vector of GyroData samples, each containing a timestamp and 6-axis IMU readings.
        */
        void HandleGyroData(const std::vector<GyroData>& data);

        /**
        * \brief Feed an exposure metadata sample into the real-time stitcher.
        * \param data ExposureData containing the timestamp and exposure duration for the current frame.
        */
        void HandleExposureData(const ExposureData& data);

    private:
        std::shared_ptr<StitcherImp> imp_;
    };
};
