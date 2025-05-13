#include "mt6701_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mt6701 {

static const char *const TAG = "mt6701";
static const uint8_t REGISTER_ANGLE = 0x03;

float position2degrees(uint16_t pos) {
  const uint16_t resolution = 0x4000;  // 14 bits
  return (pos * 360.0f) / resolution;
}

void MT6701Sensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MT6701 '%s'...", this->name_.c_str());

  if (!read_position().has_value()) {
    this->mark_failed();
    return;
  }
}

void MT6701Sensor::dump_config() {
  LOG_SENSOR("", "MT6701", this);
  LOG_UPDATE_INTERVAL(this);
  LOG_I2C_DEVICE(this);
}

void MT6701Sensor::update() {
  auto pos = read_position();
  if (!pos.has_value()) {
    this->status_set_warning();
    return;
  }
  this->publish_state(position2degrees(pos.value()));
}

optional<uint16_t> MT6701Sensor::read_position() {
  uint16_t pos = 0;
  if (!this->read_byte_16(REGISTER_ANGLE, &pos)) {
    return {};
  }
  return pos >> 2;  // 14 bits
}

}  // namespace mt6701
}  // namespace esphome
