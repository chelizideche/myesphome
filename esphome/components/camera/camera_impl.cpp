#include "camera_impl.h"
#include "camera_image_reader_impl.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

static const char *const TAG = "camera_impl";

namespace esphome {
namespace camera {

void CameraImpl::set_camera_image_spec(int width, int height, ImageFormat format) {
  this->camera_image_spec_.width = width;
  this->camera_image_spec_.height = height;
  this->camera_image_spec_.format = format;
}
void CameraImpl::set_idle_update_interval(uint32_t idle_update_interval) {
  this->idle_update_interval_ = idle_update_interval;
}
void CameraImpl::set_max_update_interval(uint32_t max_update_interval) {
  this->max_update_interval_ = max_update_interval;
}
void CameraImpl::set_encoder(Encoder *encoder) { this->encoder_ = encoder; }
void CameraImpl::set_encoder_buffer_size(size_t encoder_buffer_size) {
  this->encoder_buffer_size_ = encoder_buffer_size;
}
void CameraImpl::set_encoder_buffer_grow(size_t encoder_buffer_grow) {
  this->encoder_buffer_grow_ = encoder_buffer_grow;
}

CameraImageReader *CameraImpl::create_image_reader() {
  ESP_LOGV(TAG, "CameraImpl::create_image_reader.");
  return new CameraImageReaderImpl;
}

void CameraImpl::request_image(CameraRequester requester) {
  ESP_LOGV(TAG, "CameraImpl::request_image requester: %d", requester);
  this->image_requesters_ |= (1U << requester);
}

void CameraImpl::start_stream(CameraRequester requester) {
  ESP_LOGV(TAG, "CameraImpl::start_stream requester: %d", requester);
  if (this->stream_requesters_ & (1U << requester))
    return;

  this->stream_start_callback_.call();
  this->stream_requesters_ |= (1U << requester);
}

void CameraImpl::stop_stream(CameraRequester requester) {
  ESP_LOGV(TAG, "CameraImpl::stop_stream requester: %d", requester);
  this->stream_stop_callback_.call();
  this->stream_requesters_ &= ~(1U << requester);
}

void CameraImpl::setup() {
  this->pixels_ = std::make_shared<CameraImageImpl>();
  if (!this->pixels_->set_data_length(this->camera_image_spec_.bytes_per_image())) {
    this->status_set_error("Failed to allocate memory for image buffer.");
    this->mark_failed();
    return;
  }

  this->jpeg_ = std::make_shared<CameraImageImpl>();
  if (!this->jpeg_->set_data_length(this->encoder_buffer_size_)) {
    this->status_set_error("Failed to allocate memory for encoder buffer.");
    this->mark_failed();
    return;
  }
}

bool CameraImpl::camera_loop() {
  const uint32_t now = App.get_loop_component_start_time();

  if (state_ == CAMERA_STATE_INIT) {
    if (!this->pixels_ || !this->jpeg_) {
      this->status_set_error(
          "Missing setup() call detected. Are you overriding setup() without calling CameraImpl::setup() ?");
      this->mark_failed();
      return false;
    }

    state_ = CAMERA_STATE_WAIT_FOR_REQUEST;
  }

  if (state_ == CAMERA_STATE_WAIT_FOR_REQUEST) {
    // Request idle image every idle_update_interval
    if (this->idle_update_interval_ != 0 && now - this->last_idle_request_ > this->idle_update_interval_) {
      this->last_idle_request_ = now;
      this->request_image(camera::IDLE);
    }

    // No new image requested
    if (!image_requesters_ && !stream_requesters_)
      return false;

    state_ = CAMERA_STATE_CAPTURE_BEGIN;
  }

  if (state_ == CAMERA_STATE_CAPTURE_BEGIN) {
    this->camera_incremental_context_.reset();
    state_ = CAMERA_STATE_CAPTURING;
  }

  if (state_ == CAMERA_STATE_CAPTURING) {
    this->camera_incremental_context_.done = true;
    this->image_capture_callback_.call(this->pixels_, this->camera_image_spec_, this->camera_incremental_context_);
    // Incremental image capture
    if (!this->camera_incremental_context_.done)
      return true;

    // Check that we have a valid image for the encoder
    if (this->camera_image_spec_.bytes_per_image() != this->pixels_->get_data_length()) {
      ESP_LOGE(TAG, "Spec bytes %d != %d image bytes!", this->camera_image_spec_.bytes_per_image(),
               this->pixels_->get_data_length());
      state_ = CAMERA_STATE_CLEAR_REQUEST;
    } else {
      state_ = CAMERA_STATE_OVERLAY_BEGIN;
    }
  }

  if (state_ == CAMERA_STATE_OVERLAY_BEGIN) {
    this->camera_incremental_context_.reset();
    state_ = CAMERA_STATE_OVERLAYING;
  }

  if (state_ == CAMERA_STATE_OVERLAYING) {
    this->camera_incremental_context_.done = true;
    this->overlay_callback_.call(this->pixels_, this->camera_image_spec_, this->camera_incremental_context_);
    // Incremental image overlay
    if (!this->camera_incremental_context_.done)
      return true;

    state_ = CAMERA_STATE_ENCODE_BEGIN;
  }

  if (state_ == CAMERA_STATE_ENCODE_BEGIN) {
    // Is image still in publishing ?
    if (this->jpeg_.use_count() > 1) {
      return false;
    }

    // Set available data to max for encoder.
    this->jpeg_->set_data_length(this->jpeg_->get_max_data_length());
    state_ = CAMERA_STATE_ENCODING;
  }

  if (state_ == CAMERA_STATE_ENCODING) {
    // Encodes the pixels and returns the number of bytes written.
    size_t length = this->encoder_->encode_pixels(&camera_image_spec_, this->pixels_.get(), this->jpeg_.get());
    switch (this->encoder_->get_last_error()) {
      case ENCODER_ERROR_SUCCESS: {
        // Incremental image encoding
        if (length == 0)
          return true;

        // Image encoded
        this->jpeg_->set_data_length(length);
        state_ = CAMERA_STATE_RATE_LIMITING;
      } break;
      case ENCODER_ERROR_OUT_OF_MEMORY: {
        ESP_LOGW(TAG, "The encoder buffer is too small. The encoding failed. Buffer size: %u",
                 this->jpeg_->get_max_data_length());
        state_ = CAMERA_STATE_CLEAR_REQUEST;
        if (this->encoder_buffer_grow_ > 0) {
          size_t new_size = this->jpeg_->get_max_data_length() + this->encoder_buffer_grow_;
          ESP_LOGI(TAG, "Increasing encoder buffer size with %u bytes. Retry...", this->encoder_buffer_grow_);
          if (!this->jpeg_->set_data_length(new_size)) {
            this->status_set_error("Failed to increase encoder buffer.");
            this->mark_failed();
            return false;
          }

          // Retry encoding with more memory. Encoder resets itself in case of an error.
          state_ = CAMERA_STATE_ENCODING;
          return true;
        }
      } break;
      default:
        ESP_LOGE(TAG, "Encoder failed with error code: %u", this->encoder_->get_last_error());
        state_ = CAMERA_STATE_CLEAR_REQUEST;
    }
  }

  if (state_ == CAMERA_STATE_RATE_LIMITING) {
    if (now - this->last_update_ <= this->max_update_interval_)
      return false;

    state_ = CAMERA_STATE_PUBLISHING;
  }

  if (state_ == CAMERA_STATE_PUBLISHING) {
    this->jpeg_->set_requesters(this->image_requesters_ | this->stream_requesters_);
    this->new_image_callback_.call(this->jpeg_);
    this->last_update_ = now;
    state_ = CAMERA_STATE_CLEAR_REQUEST;
  }

  if (state_ == CAMERA_STATE_CLEAR_REQUEST) {
    this->image_requesters_ = 0;
    state_ = CAMERA_STATE_WAIT_FOR_REQUEST;
  }

  return false;
}

void CameraImpl::loop() { camera_loop(); }

void CameraImpl::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Camera:\n"
                "  Name: %s\n"
                "  Internal: %s\n"
                "  Resolution: %dx%d\n"
                "  Format: %d\n"
                "  Encoder enabled: %s\n"
                "  Encoder size: %d\n"
                "  Encoder grow: %d\n"
                "  Idle update: %d ms\n"
                "  Max update: %d ms",
                this->name_.c_str(), YESNO(this->is_internal()), camera_image_spec_.width, camera_image_spec_.height,
                camera_image_spec_.format, YESNO(this->encoder_), encoder_buffer_size_, encoder_buffer_grow_,
                idle_update_interval_, max_update_interval_);
}

}  // namespace camera
}  // namespace esphome
