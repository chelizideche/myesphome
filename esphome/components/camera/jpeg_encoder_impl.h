#pragma once

#ifdef USE_CAMERA_SW_JPEG_ENCODER

#include "encoder.h"

#include <JPEGENC.h>

namespace esphome {
namespace camera {

class JPEGEncoderImpl : public Encoder {
 public:
  // -------- Encoder --------
  void set_quality(EncoderQuality quality) override;
  void set_subsampling(EncoderSubsampling subsampling) override;
  void set_mcu_count(size_t mcu_count) override;

  size_t encode_pixels(CameraImageSpec *spec, CameraImage *pixels, CameraImage *jpeg) override;
  EncoderError get_last_error() override;
  // -------------------------

 protected:
  int to_internal(EncoderQuality quality);
  int to_internal(ImageFormat format);
  int to_internal(EncoderSubsampling subsampling);

  int quality_{0};
  int subsampling_{0};
  JPEGENC encoder_;
  JPEGENCODE encoder_state_;
  bool incremental_{false};
  ssize_t mcu_count_;
};

}  // namespace camera
}  // namespace esphome

#endif
