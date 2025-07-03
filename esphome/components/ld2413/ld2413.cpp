#include "ld2413.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ld2413 {

static const char *const TAG = "ld2413";

void LD2413Sensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up LD2413 Sensor...");
  this->last_publish_ = millis();
}

void LD2413Sensor::loop() {
  while (available() >= 14) {
    uint8_t buffer[14];
    read_array(buffer, 14);

    if (memcmp(buffer, "\xF4\xF3\xF2\xF1", 4) == 0) {
      float distance_mm;
      memcpy(&distance_mm, &buffer[6], sizeof(float));
      latest_distance_cm_ = distance_mm / 10.0f;
    }
  }
}

void LD2413Sensor::update() {
  const uint32_t current_time = millis();

  if (current_time - last_publish_ >= this->get_update_interval()) {
    if (!isnan(latest_distance_cm_)) {
      ESP_LOGD(TAG, "Distance: %.2f cm", latest_distance_cm_);
      this->publish_state(latest_distance_cm_);
    }
    last_publish_ = current_time;
  }
}

}  // namespace ld2413
}  // namespace esphome
