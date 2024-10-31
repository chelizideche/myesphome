#include "gp8211.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gp8211 {

static const char *const TAG = "gp8211";

static const uint8_t RANGE_REGISTER = 0x01;

void GP8211::setup() {
    this->write_register(RANGE_REGISTER, reinterpret_cast<uint8_t*>(&this->voltage_), sizeof(this->voltage_));
}

void GP8211::dump_config() {
  ESP_LOGCONFIG(TAG, "GP8211:");
  ESP_LOGCONFIG(TAG, "  Voltage: %s Mode activated", this->voltage_ == GP8211_VOLTAGE_10V ? "10V" : "5V");
  LOG_I2C_DEVICE(this);
}

}  // namespace gp8211
}  // namespace esphome
