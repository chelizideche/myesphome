#ifdef USE_CAMERA_SW_JPEG_ENCODER

#include "jpeg_encoder_impl.h"

namespace esphome {
namespace camera {

void JPEGEncoderImpl::set_quality(EncoderQuality quality) { this->quality_ = to_internal(quality); }

void JPEGEncoderImpl::set_subsampling(EncoderSubsampling subsampling) { this->subsampling_ = to_internal(subsampling); }

void JPEGEncoderImpl::set_mcu_count(size_t mcu_count) { this->mcu_count_ = mcu_count; }

size_t JPEGEncoderImpl::encode_pixels(CameraImageSpec *spec, CameraImage *pixels, CameraImage *jpeg) {
  uint8_t *buffer = jpeg->get_data_buffer();
  size_t buffer_length = jpeg->get_data_length();

  if (!this->incremental_) {
    if (encoder_.open(buffer, buffer_length) != JPEGE_SUCCESS)
      return 0;

    if (encoder_.encodeBegin(&encoder_state_, spec->width, spec->height, to_internal(spec->format), this->subsampling_,
                             this->quality_) != JPEGE_SUCCESS)
      return 0;

    this->incremental_ = this->mcu_count_ > 0;
  }

  int bpr = spec->bytes_per_row();
  int bpp = spec->bytes_per_pixel();
  int rc = JPEGE_SUCCESS;
  ssize_t mcus = this->mcu_count_;
  while (encoder_state_.y < spec->height && rc == JPEGE_SUCCESS && (this->mcu_count_ == 0 || mcus > 0)) {
    rc = encoder_.addMCU(&encoder_state_,
                         &pixels->get_data_buffer()[(encoder_state_.x * bpp) + (encoder_state_.y * bpr)], bpr);
    --mcus;
  }

  if (rc != JPEGE_SUCCESS) {
    this->incremental_ = false;
    return 0;
  }

  this->incremental_ = encoder_state_.y < spec->height;
  if (!this->incremental_)
    return encoder_.close();

  return 0;
}

EncoderError JPEGEncoderImpl::get_last_error() {
  switch (encoder_.getLastError()) {
    case JPEGE_SUCCESS:
      return ENCODER_ERROR_SUCCESS;
    case JPEGE_INVALID_PARAMETER:
      return ENCODER_ERROR_INVALID_PARAMETER;
    case JPEGE_ENCODE_ERROR:
      return ENCODER_ERROR_ENCODE_ERROR;
    case JPEGE_MEM_ERROR:
      return ENCODER_ERROR_MEMORY_ERROR;
    case JPEGE_NO_BUFFER:
      return ENCODER_ERROR_OUT_OF_MEMORY;
    case JPEGE_UNSUPPORTED_FEATURE:
      return ENCODER_ERROR_UNSUPPORTED_FEATURE;
    default:
      return ENCODER_ERROR_GENERIC;
  }

  return ENCODER_ERROR_GENERIC;
}

int JPEGEncoderImpl::to_internal(EncoderQuality quality) {
  switch (quality) {
    case ENCODER_QUALITY_BEST:
      return JPEGE_Q_BEST;
    case ENCODER_QUALITY_HIGH:
      return JPEGE_Q_HIGH;
    case ENCODER_QUALITY_MED:
      return JPEGE_Q_MED;
    case ENCODER_QUALITY_LOW:
      return JPEGE_Q_LOW;
  }

  return JPEGE_Q_BEST;
}

int JPEGEncoderImpl::to_internal(ImageFormat format) {
  switch (format) {
    case IMAGE_FORMAT_GRAYSCALE:
      return JPEGE_PIXEL_GRAYSCALE;
    case IMAGE_FORMAT_RGB565:
      return JPEGE_PIXEL_RGB565;
    case IMAGE_FORMAT_RGB888:
      return JPEGE_PIXEL_RGB888;
  }

  return JPEGE_PIXEL_RGB888;
}

int JPEGEncoderImpl::to_internal(EncoderSubsampling sampling) {
  switch (sampling) {
    case ENCODER_SUBSAMPLING_444:
      return JPEGE_SUBSAMPLE_444;
    case ENCODER_SUBSAMPLING_420:
      return JPEGE_SUBSAMPLE_420;
  }

  return JPEGE_SUBSAMPLE_444;
}

}  // namespace camera
}  // namespace esphome

#endif
