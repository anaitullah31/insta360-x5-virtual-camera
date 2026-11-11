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

    /**
     * \brief Set the root directory where AI/ML model files are located.
     *        Required before using features such as AI-Stitching or ColorPlus.
     * \param root_dir Absolute path to the directory containing model files.
     */
    void MEDIASDK_API SetModelFileRootDir(const std::string& root_dir);

    /**
     * \brief Query metadata and basic properties of one or more media files.
     * \param file_paths List of input file paths (e.g. two paths for a dual-lens 5.7k clip).
     * \param info       Output struct populated with media type, resolution, fps, bitrate and duration.
     * \return true if the file(s) were parsed successfully, false otherwise.
     */
    bool MEDIASDK_API GetMediaFileInfo(const std::vector<std::string>& file_paths, MediaFileInfo& info);

    class MEDIASDK_API VideoStitcher {
    public:
        /**
         * \brief Construct a VideoStitcher instance with default settings.
         *        Call SetInputPath() and SetOutputPath() before calling StartStitch().
         */
        VideoStitcher();

        /**
         * \brief Destructor. Releases all internal resources. Any in-progress stitch is cancelled first.
         */
        ~VideoStitcher() = default;

    public:
        /**
        *\brief get timestamp list
        *\param the map contains frame index and frame timestamps
        */
        //static std::map<uint64_t, uint64_t> GetFrameTimeStampsList(const std::string& url);

        /**
         * \brief set the path of the source that you wanted.
         * \param input_paths if the source is the kind that '5.7k', the size of the vector is 2, otherwise
         */
        void SetInputPath(const std::vector<std::string>& input_paths);

        /**
        *\brief set output path of the stab path
        */
        void SetStabDataOutputPath(const std::string& file_path);

        /**
        *\brief set the info of imgage sequence
        *\param output_dir the output dir of imgage sequence. does not include filename
        *\param image_type support jpg and png
        */
        void SetImageSequenceInfo(const std::string& output_dir, IMAGE_TYPE image_type);

        /**
        *\brief set the numbers of export frame
        *\param vec the vector that saved frame index, starting from zero
        */
        void SetExportFrameSequence(const std::vector<uint64_t>& vec);

        /*
        * \brief If you have a CUDA-accelerated environment,
        *\       you can set this parameter to true. It can provide the speed of stitching
        */
        void EnableCuda(bool enable);

        /**
         *\brief set the file path of the output that you wanted, include filename
         */
        void SetOutputPath(const std::string& output_path);

        /**
         *\brief set the video bitrate of the output file bps
         */
        void SetOutputBitRate(int64_t bitRate);

        /**
         * \brief set the video resolution of the output file, default size of source
         */
        void SetOutputSize(int width, int height);

        /**
         * \brief Whether to turn on FlowState
         * \param enable true is on and false is off, default off
         */
        void EnableFlowState(bool enable);

        /**
         * \brief Whether to turn on directionLock, the feautre depend on flowstate, flowstate must open if you want use the feature
         * \param enable true is on and false is off, default off
         * \
         */
        void EnableDirectionLock(bool enable);

        /**
         * \brief Whether to turn on ImageFusion,  not support TEMPLATE stitch
         * \param enable  true is on and false is off, default off
         */
        void EnableStitchFusion(bool enable);

        /**
         * \brief Whether to turn on cooling shell detection
         * \param enable  true is on and false is off, default off
         */
        void EnableCoolingShellDetection(bool enable);

        /**
         * \brief Whether to turn on denoise
         * \param enable  true is on and false is off, default off
         */
        void EnableDenoise(bool enable);

        /**
         * \brief Whether to turn on ImageFusion
         * \param enable  true is on and false is off, default off
         * \param model_path this param is required for the feature, if the param is empty, the feature is disabled
         * \param strength
         */
        void EnableColorPlus(bool enable, float strength = 1.0f);

        /**
         * \brief Whether to use h265 encoder, default encoder is h264
         * \param enable true = h265, false = h264 (default)
         */
        void EnableH265Encoder(bool enable);

        /**
         * \brief Whether to export 10-bit video. Default is false (8-bit).
         * \param enable true = request 10-bit export; false = 8-bit (default)
         * \note Actual output bit depth depends on the source material: only when
         *       the source is 10-bit (e.g. C9/X6 10-bit footage) will 10-bit output
         *       be produced. If the source is 8-bit, 10-bit export is silently
         *       downgraded to 8-bit with a log warning.
         *       10-bit export is recommended with H.265 (EnableH265Encoder) since
         *       H.264 does not support 10-bit. If H.264 is set when 10-bit is
         *       requested, the encoder will be automatically switched to H.265.
         */
        void Enable10BitExport(bool enable);

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
         *  \brief set the callback that can tell the client the progress of stitching
         *  \param callback used for get process the process
         *  \param lcontent the context
         */
        void SetStitchProgressCallback(stitch_process_callback callback);

        /**
         *  \brief set the callback that can tell the client the error if stitcher goes error
         *  \param callback used for get err info of the process.
         *  \param lcontent the context
         */
        void SetStitchStateCallback(stitch_error_callback callback);

        /**
         *  \brief start stitch
         */
        void StartStitch();

        /**
         *  \brief cancel stitch
         */
        bool CancelStitch();

        /**
        *  \brief get stitch  progress  0 ~ 100
        */
        int GetStitchProgress() const;

        /**
        * \brief Returns a map of runtime feature status values after stitching.
        *        Each entry maps a feature name to its actual runtime status:
        *        -1=unknown, 0=off, 1=on, 2=skipped(auto), 3=failed.
        */
        std::map<std::string, int> GetFeatureStatusMap() const;

        /**
        * \brief set CameraAccessoryType
        */
        void SetCameraAccessoryType(CameraAccessoryType type);

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
         * \brief set image process accel type
         * @param type
         */
        void SetImageProcessingAccelType(ImageProcessingAccel type);

        /**
        * \brief enable Defringe
        */
        void EnableDefringe(bool enable);

        /**
         * \brief set exposure
         * \param exposure range[-100,100]
         */
        void SetExposure(int exposure);

        /**
         * \brief set highlights
         * \param highlights range[-100,100]
         */
        void SetHighlights(int highlights);

        /**
         * \brief set shadows
         * \param shadows range[-100,100]
        */
        void SetShadows(int shadows);

        /**
         * \brief set contrast
         * \param contrast range[-100,100]
        */
        void SetContrast(int contrast);

        /**
         * \brief set brightness
         * \param brightness range[-100,100]
         */
        void SetBrightness(int brightness);

        /**
         * \brief set blackpoint
         * \param blackpoint range[-100,100]
         */
        void SetBlackpoint(int blackpoint);

        /**
         * \brief set saturation
         * \param saturation range[-100,100]
         */
        void SetSaturation(int saturation);

        /**
         * \brief set vibrance
         * \param vibrance range[-100,100]
        */
        void SetVibrance(int vibrance);

        /**
         * \brief set warmth
         * \param warmth range[-100,100]
         */
        void SetWarmth(int warmth);

        /**
         * \brief set tint
         * \param tint range[-100,100]
         */
        void SetTint(int tint);

        /**
         * \brief set definition
         * \param contrast range[0, 100]
         */
        void SetDefinition(int definition);

    private:
        std::shared_ptr<StitcherImp> imp_;
    };

    /**
     * \class ImageStitcher
     * \brief A class that stitch image, only support the formats that maked by Insta360 camera, for example *.insp
     */
    class MEDIASDK_API ImageStitcher {
    public:
        ImageStitcher();
        ~ImageStitcher() = default;

    public:
        /**
        * \brief set the path of the source that you wanted.
        * \param input_path
        */
        void SetInputPath(const std::vector<std::string>& input_paths);

        /**
         * \brief set the path of the output that you wanted
         */
        void SetOutputPath(const std::string& output_path);

        /**
         * \brief set the video resolution of the output file
         */
        void SetOutputSize(int width, int height);

        /**
         * \brief Whether to turn on FlowState
         * \param enable true is on and false is off default off
         */
        void EnableFlowState(bool enable);

        /**
         * \brief Whether to turn on ImageFusion,  not support TEMPLATE stitch
         * \param enable true is on and false is off, default off
         * \param model_path if model file is not found, colorplus is disabled
         */
        void EnableColorPlus(bool enable, float strength = 0.3f);

        /**
        * \brief Whether to turn on denoise
        * \param enable  true is on and false is off, default off
        */
        void EnableDenoise(bool enable);

        /**
        * \brief set the type of stitch,
        * \param type  it has four kind that has template,optflow,dynamicStitch,AI-Stitching and
        * \            template is fastest and AI-Stitching is slowest.
        * \            When the stitch type is AI-Stitching, the AI-Stitching model file must be set.
        * \            You can refer to this interface 'SetAiStitchModelFile ',
        * \            otherwise Ai-Stitching will not take effect
        */
        void SetStitchType(STITCH_TYPE type);

        /*
        * \brief If you have a CUDA-accelerated environment,
        *\       you can set this parameter to true. It can provide the speed of stitching
        */
        void EnableCuda(bool enable);

        /**
         * \brief set image process accel type
         * @param type
         */
        void SetImageProcessingAccelType(ImageProcessingAccel type);

        /**
         * \brief Whether to turn on ImageFusion,  not support TEMPLATE stitch
         * \param enable  true is on and false is off, default off
         */
        void EnableStitchFusion(bool enable);

        /**
         * \brief Whether to turn on cooling shell detection
         * \param enable  true is on and false is off, default off
         */
        void EnableCoolingShellDetection(bool enable);

        /**
        * \brief set CameraAccessoryType
        */
        void SetCameraAccessoryType(CameraAccessoryType type);

        /**
         * \brief start stitch, this function is called synchronously.
         */
        bool Stitch();

        /**
         * \brief set exposure
         * \param exposure range[-100,100]
         */
        void SetExposure(int exposure);

        /**
         * \brief set highlights
         * \param highlights range[-100,100]
         */
        void SetHighlights(int highlights);

        /**
         * \brief set shadows
         * \param shadows range[-100,100]
        */
        void SetShadows(int shadows);

        /**
         * \brief set contrast
         * \param contrast range[-100,100]
        */
        void SetContrast(int contrast);

        /**
         * \brief set brightness
         * \param brightness range[-100,100]
         */
        void SetBrightness(int brightness);

        /**
         * \brief set blackpoint
         * \param blackpoint range[-100,100]
         */
        void SetBlackpoint(int blackpoint);

        /**
         * \brief set saturation
         * \param saturation range[-100,100]
         */
        void SetSaturation(int saturation);

        /**
         * \brief set vibrance
         * \param vibrance range[-100,100]
        */
        void SetVibrance(int vibrance);

        /**
         * \brief set warmth
         * \param warmth range[-100,100]
         */
        void SetWarmth(int warmth);

        /**
         * \brief set tint
         * \param tint range[-100,100]
         */
        void SetTint(int tint);

        /**
         * \brief set definition
         * \param contrast range[0, 100]
         */
        void SetDefinition(int definition);
    private:
        std::shared_ptr<StitcherImp> imp_;
    };
};
