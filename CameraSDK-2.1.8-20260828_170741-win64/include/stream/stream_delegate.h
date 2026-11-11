//
//  stream_delegate.h
//  CameraSDK
//
//  Created by capjason on 2020/3/18.
//

#ifndef stream_delegate_h
#define stream_delegate_h
#include "stream_types.h"
#include <camera/ins_types.h>

namespace ins_camera {
	class CAMERASDK_API StreamDelegate {
	public:
		/**
		 * @brief Callback for audio data
		 * @param data audio data
		 * @param size size of the audio data
		 * @param timeStamp timestamp of the audio data
		 */
		virtual void OnAudioData(const uint8_t* data, size_t size, int64_t timestamp) = 0;

		/**
		* @brief Callback for video data
		* @param data video data (h264 or h265 format)
		* @param size size of the video data
		* @param timeStamp timestamp of the video data
		* @param streamType not relevant, can be ignored
		* @param stream_index for dual-stream, use this parameter to distinguish the streams
		*/
		virtual void OnVideoData(const uint8_t* data, size_t size, int64_t timestamp, uint8_t streamType, int stream_index = 0) = 0;

		/**
		* @brief Callback for gyro/stabilization data
		* @param data array of gyro/stabilization data, see 'GyroData' for details
		*/
		virtual void OnGyroData(const std::vector<GyroData>& data) = 0;

		/**
		* @brief Callback for exposure data
		* @param data exposure data, see 'ExposureData' for details
		*/
		virtual void OnExposureData(const ExposureData& data) = 0;
	};
}

#endif /* stream_delegate_h */
