#include "ld2413.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ld2413 {

static const char *const TAG = "ld2413";

void LD2413Sensor::setup() { ESP_LOGCONFIG(TAG, "Setting up LD2413 Sensor..."); }

void LD2413Sensor::update() { this->read_sensor_(); }

void LD2413Sensor::read_sensor_() {
  const uint8_t header[4] = {0xF4, 0xF3, 0xF2, 0xF1};

  while (available() >= 14) {
    uint8_t buffer[14];
    read_array(buffer, 14);

    if (memcmp(buffer, header, 4) == 0) {
      float distance_mm;
      memcpy(&distance_mm, &buffer[6], sizeof(float));

      float distance_cm = distance_mm / 10.0f;
      ESP_LOGD(TAG, "Distance: %.2f cm", distance_cm);
      this->publish_state(distance_cm);
    } else {
      ESP_LOGW(TAG, "Invalid frame: %s", format_hex_pretty(buffer, 14).c_str());
    }
  }
}

}  // namespace ld2413
}  // namespace esphome
