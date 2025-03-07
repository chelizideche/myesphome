#include "mcp4151.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp4151 {

static const char *const TAG = "mcp4151";

void MCP4151::setup() {
  this->spi_setup();
  ESP_LOGCONFIG(TAG, "MCP4151 Setup");
}

void MCP4151::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP4151 digital potentiometer");
  ESP_LOGCONFIG(TAG, "  last value: %d", static_cast<int>(this->last_value_));
}

void MCP4151::write_state(float state) {
  // scale 0..1 => 0..255
  uint8_t value = static_cast<uint8_t>(state * 255.0f);
  ESP_LOGD(TAG, "  set value: %d", static_cast<int>(value));
  this->last_value_ = value;
  this->send_command(value);
}

void MCP4151::send_command(uint8_t value) {
  this->enable();
  this->transfer_byte(0x00); // command Byte (Pot #0)
  this->transfer_byte(value);
  this->disable();
}

}  // namespace mcp4151
}  // namespace esphome
