#pragma once

#include "camera.h"

namespace esphome {
namespace camera {

/** Camera image interface implemenation.
 */
class CameraImageImpl : public CameraImage {
 public:
  // Specifies the filter used in add_image_callback.
  void set_requesters(uint8_t requesters);
  // Sets the length of the image or jpeg buffer data. Can be used to increase the buffer.
  // returns false if allocation failed.
  bool set_data_length(size_t data_length);
  // Returns the allocated size of the buffer.
  size_t get_max_data_length();

  // ---- CameraImage interface ----
  uint8_t *get_data_buffer() override;
  size_t get_data_length() override;
  bool was_requested_by(CameraRequester requester) const override;
  // -------------------------------
  ~CameraImageImpl() override;

 protected:
  RAMAllocator<uint8_t> allocator_;
  uint8_t *data_{nullptr};
  size_t capacity_{0};
  size_t length_{0};
  uint8_t requesters_{0};
};

}  // namespace camera
}  // namespace esphome
