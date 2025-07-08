#pragma once

#include "camera.h"
#include "camera_image_impl.h"
#include "encoder.h"

namespace esphome {
namespace camera {

/** Camera interface implemenation.
 */
class CameraImpl : public Camera {
 public:
  enum CameraState : uint8_t {
    CAMERA_STATE_INIT = 0,
    CAMERA_STATE_WAIT_FOR_REQUEST,
    CAMERA_STATE_CAPTURE_BEGIN,
    CAMERA_STATE_CAPTURING,
    CAMERA_STATE_OVERLAY_BEGIN,
    CAMERA_STATE_OVERLAYING,
    CAMERA_STATE_ENCODE_BEGIN,
    CAMERA_STATE_ENCODING,
    CAMERA_STATE_RATE_LIMITING,
    CAMERA_STATE_PUBLISHING,
    CAMERA_STATE_CLEAR_REQUEST,
  };

  // Sets the camera's image specifications
  void set_camera_image_spec(int width, int height, ImageFormat format);
  // Sets the update interval in milliseconds for images without requests
  void set_idle_update_interval(uint32_t idle_update_interval);
  // Sets the number of milliseconds between two consecutive images
  void set_max_update_interval(uint32_t max_update_interval);
  // Sets encoder implementation
  void set_encoder(Encoder *encoder);
  // Sets the initial number of bytes in the encoder buffer
  void set_encoder_buffer_size(size_t encoder_buffer_size);
  // Specifies how many bytes the buffer should grow after an underflow error
  void set_encoder_buffer_grow(size_t encoder_buffer_grow);
  // Performs camera processing tasks such as image capture and JPEG encoding spread
  // over multiple loop cycles to avoid blocking and ensure real-time responsiveness.
  // This method is intended to be called repeatedly from the loop.
  // It performs a portion of the image capture or encoding process each time it runs.
  // Returns true if additional processing is needed and the loop should continue running.
  // Returns false if all camera-related tasks are complete.
  bool camera_loop();
  // ---- Camera interface ----
  CameraImageReader *create_image_reader() override;
  void request_image(CameraRequester requester) override;
  void start_stream(CameraRequester requester) override;
  void stop_stream(CameraRequester requester) override;
  // -------- Component -------
  void setup() override;
  void loop() override;
  void dump_config() override;
  // --------------------------
 protected:
  std::shared_ptr<CameraImageImpl> pixels_;
  std::shared_ptr<CameraImageImpl> jpeg_;
  CameraImageSpec camera_image_spec_{0};
  CameraIncrementalContext camera_incremental_context_;
  CameraState state_{CAMERA_STATE_INIT};
  Encoder *encoder_{nullptr};
  uint8_t image_requesters_{0};
  uint8_t stream_requesters_{0};
  size_t encoder_buffer_size_{0};
  size_t encoder_buffer_grow_{0};
  uint32_t last_idle_request_{0};
  uint32_t idle_update_interval_{0};
  uint32_t last_update_{0};
  uint32_t max_update_interval_{0};
};

}  // namespace camera
}  // namespace esphome
