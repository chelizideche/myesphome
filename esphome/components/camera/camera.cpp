#include "camera.h"

namespace esphome {
namespace camera {

Camera *Camera::global_camera = nullptr;

Camera::Camera() {
  if (global_camera != nullptr) {
    mark_failed();
    return;
  }

  global_camera = this;
}

void Camera::add_image_callback(std::function<void(std::shared_ptr<CameraImage>)> &&callback) {
  this->new_image_callback_.add(std::move(callback));
}

void Camera::add_stream_start_callback(std::function<void()> &&callback) {
  this->stream_start_callback_.add(std::move(callback));
}

void Camera::add_stream_stop_callback(std::function<void()> &&callback) {
  this->stream_stop_callback_.add(std::move(callback));
}

Camera *Camera::instance() { return global_camera; }

}  // namespace camera
}  // namespace esphome
