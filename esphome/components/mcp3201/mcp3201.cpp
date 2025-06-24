#include "mcp3201.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp3201 {

static const char *const TAG = "mcp3201";

float MCP3201::get_setup_priority() const { return setup_priority::HARDWARE; }

void MCP3201::setup() {
  ESP_LOGCONFIG(TAG, "Setting up mcp3201");
  this->spi_setup();
}

void MCP3201::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP3201:");
  LOG_PIN("  CS Pin:", this->cs_);
  ESP_LOGCONFIG(TAG, "  Reference Voltage: %.2fV", this->reference_voltage_);
}

float MCP3201::read_data() {
  this->enable();
  uint8_t byte0 = this->transfer_byte(0x00);
  uint8_t byte1 = this->transfer_byte(0x00);
  this->disable();
  uint16_t digital_value = (((byte0 & 0b00011111) << 7) | (byte1 >> 1));
  return float(digital_value) / 4096.000 * this->reference_voltage_;
}

}  // namespace mcp3201
}  // namespace esphome
