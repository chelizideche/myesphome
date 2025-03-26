#include "ads1100_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ads1100 {

static const char *const TAG = "ads1100.sensor";

void ADS1100Sensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADS1100 Sensor...");
  if (!this->parent_->write_byte(0x00, (this->gain_ << 2) | (this->data_rate_ & 0x03))) {
    this->mark_failed();
    return;
  }
}

void ADS1100Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "ADS1100 Sensor:");
  LOG_I2C_DEVICE(this->parent_);
  ESP_LOGCONFIG(TAG, "  Gain: %d", this->gain_);
  ESP_LOGCONFIG(TAG, "  Data Rate: %d", this->data_rate_);
  LOG_SENSOR("  ", "Voltage", this);
}

void ADS1100Sensor::update() {
  if (!this->parent_->write_byte(0x00, (this->gain_ << 2) | (this->data_rate_ & 0x03))) {
    this->status_set_warning();
    return;
  }

  // Wait for conversion to complete
  delay(10);

  uint8_t data[2];
  if (!this->parent_->read_bytes(0x00, data, 2)) {
    this->status_set_warning();
    return;
  }

  int16_t raw = (data[0] << 8) | data[1];
  float voltage = (raw * 2.048f) / 32768.0f * this->gain_;

  this->publish_state(voltage);
  this->status_clear_warning();
}

}  // namespace ads1100
}  // namespace esphome