#pragma once

#include "camera.h"

namespace esphome {
namespace camera {

/** Enumeration of different qualities of the encoded image.
 *  Better quality results in more memory consumption and higher CPU usage.
 */
enum EncoderQuality : uint8_t {
  ENCODER_QUALITY_BEST = 0,
  ENCODER_QUALITY_HIGH,
  ENCODER_QUALITY_MED,
  ENCODER_QUALITY_LOW
};

/** Enumeration of various errors during encoding.
 */
enum EncoderError : uint8_t {
  ENCODER_ERROR_SUCCESS = 0,
  ENCODER_ERROR_INVALID_PARAMETER,
  ENCODER_ERROR_ENCODE_ERROR,
  ENCODER_ERROR_MEMORY_ERROR,
  ENCODER_ERROR_OUT_OF_MEMORY,
  ENCODER_ERROR_UNSUPPORTED_FEATURE,
  ENCODER_ERROR_GENERIC
};

/** Enumeration of different modes to reduce the color information in the image.
 * ENCODER_SUBSAMPLING_444 is without reduction.
 * ENCODER_SUBSAMPLING_420 is with reduced color information and lower memory consumption at the cost of reduced image
 * quality.
 */
enum EncoderSubsampling : uint8_t { ENCODER_SUBSAMPLING_444 = 0, ENCODER_SUBSAMPLING_420 };

/** Abstract base class for image encoder interfaces.
 */
class Encoder {
 public:
  // Set the quality of the encoded image.
  virtual void set_quality(EncoderQuality quality) = 0;
  // Specify whether the color information should be reduced to reduce memory consumption at the expense of quality.
  virtual void set_subsampling(EncoderSubsampling subsampling) = 0;
  // MCU_count number of MCUs to encode in every encoding call. Encoding will continue sequentially from the current MCU
  // position. A zero value disables incremental encoding and causes the full image to be encoded in a single call.
  virtual void set_mcu_count(size_t mcu_count) = 0;
  // Encodes the pixels and returns the number of bytes written.
  // Spec contains information about the format of the pixel image.
  virtual size_t encode_pixels(CameraImageSpec *spec, CameraImage *pixels, CameraImage *jpeg) = 0;
  // Return the result of the last encoding.
  virtual EncoderError get_last_error() = 0;
  virtual ~Encoder() {}
};

}  // namespace camera
}  // namespace esphome
