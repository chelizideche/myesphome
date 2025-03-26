#include "ads1100_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ads1100 {

static const char *const TAG = "ads1100.sensor";

void ADS1100Sensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADS1110 Sensor...");
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "ADS1110 parent not set!");
    this->mark_failed();
    return;
  }

  // Note: We intentionally don't configure gain and sample rate here,
  // since the ADS1110 ignores configuration attempts in many cases.
  // We just use the default device settings.
}

void ADS1100Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "ADS1110 Sensor:");
  LOG_SENSOR("  ", "Voltage", this);
}

void ADS1100Sensor::update() {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "Parent component not set!");
    this->status_set_error();
    return;
  }

  float value = this->parent_->request_measurement();
  if (std::isnan(value)) {
    ESP_LOGE(TAG, "Failed to get measurement from parent component");
    this->status_set_error();
    return;
  }

  ESP_LOGD(TAG, "Got measurement: %.3fV", value);
  this->publish_state(value);
  this->status_clear_error();
}

}  // namespace ads1100
}  // namespace esphome