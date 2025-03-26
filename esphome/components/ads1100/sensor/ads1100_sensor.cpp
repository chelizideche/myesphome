#include "ads1100_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ads1100 {

static const char *const TAG = "ads1100.sensor";

void ADS1100Sensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADS1100 Sensor...");

  // Configure the ADS1100
  uint8_t config = (this->gain_ << 2) | (this->data_rate_ & 0x03);
  ESP_LOGD(TAG, "Writing config byte: 0x%02X", config);

  if (!this->parent_->write_byte(0x00, config)) {
    ESP_LOGE(TAG, "Failed to write config byte");
    this->mark_failed();
    return;
  }

  // Verify the configuration
  uint8_t read_config;
  if (!this->parent_->read_byte(0x00, &read_config)) {
    ESP_LOGE(TAG, "Failed to read config byte");
    this->mark_failed();
    return;
  }

  if (read_config != config) {
    ESP_LOGE(TAG, "Config verification failed. Expected: 0x%02X, Got: 0x%02X", config, read_config);
    this->mark_failed();
    return;
  }

  ESP_LOGD(TAG, "ADS1100 configured successfully");
}

void ADS1100Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "ADS1100 Sensor:");
  ESP_LOGCONFIG(TAG, "  Gain: %d", this->gain_);
  ESP_LOGCONFIG(TAG, "  Data Rate: %d", this->data_rate_);
  LOG_SENSOR("  ", "Voltage", this);
}

void ADS1100Sensor::update() {
  // Configure for reading
  uint8_t config = (this->gain_ << 2) | (this->data_rate_ & 0x03);
  if (!this->parent_->write_byte(0x00, config)) {
    ESP_LOGE(TAG, "Failed to write config byte");
    this->status_set_warning();
    return;
  }

  // Wait for conversion to complete (depends on data rate)
  delay(1000 / this->data_rate_ + 1);  // Add 1ms margin

  // Read the conversion result
  uint8_t data[2];
  if (!this->parent_->read_bytes(0x00, data, 2)) {
    ESP_LOGE(TAG, "Failed to read conversion result");
    this->status_set_warning();
    return;
  }

  // Convert to voltage
  int16_t raw = (data[0] << 8) | data[1];  // MSB first
  float voltage = (raw * 2.048f) / 32768.0f * this->gain_;

  ESP_LOGD(TAG, "Raw value: %d, Voltage: %.3fV", raw, voltage);
  this->publish_state(voltage);
  this->status_clear_warning();
}

}  // namespace ads1100
}  // namespace esphome