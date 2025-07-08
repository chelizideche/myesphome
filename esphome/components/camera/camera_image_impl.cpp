#include "camera_image_impl.h"

namespace esphome {
namespace camera {

void CameraImageImpl::set_requesters(uint8_t requesters) { this->requesters_ = requesters; }

bool CameraImageImpl::set_data_length(size_t data_length) {
  if (data_length > this->capacity_) {
    uint8_t *p = this->allocator_.reallocate(this->data_, data_length);
    if (p == nullptr)
      return false;

    this->data_ = p;
    this->capacity_ = data_length;
  }
  this->length_ = data_length;
  return true;
}

size_t CameraImageImpl::get_max_data_length() { return this->capacity_; }

uint8_t *CameraImageImpl::get_data_buffer() { return this->data_; }

size_t CameraImageImpl::get_data_length() { return this->length_; }

bool CameraImageImpl::was_requested_by(camera::CameraRequester requester) const {
  return (this->requesters_ & (1 << requester)) != 0;
}

CameraImageImpl::~CameraImageImpl() {
  if (this->data_ != nullptr)
    this->allocator_.deallocate(this->data_, this->capacity_);
}

}  // namespace camera
}  // namespace esphome
