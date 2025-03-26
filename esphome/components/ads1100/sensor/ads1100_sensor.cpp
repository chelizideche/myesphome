#include "ads1100_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ads1100 {

static const char *const TAG = "ads1100.sensor";

void ADS1100Sensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADS1100 Sensor...");
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "ADS1100 parent not set!");
    this->mark_failed();
    return;
  }

  ESP_LOGD(TAG, "Current sensor settings - Gain: %d, Sample Rate: %d", this->gain_, this->sample_rate_);
  ESP_LOGD(TAG, "Parent component settings - Gain: %d, Sample Rate: %d", this->parent_->get_gain(),
           this->parent_->get_sample_rate());

  // Propagate settings to parent component
  this->parent_->set_gain(static_cast<ADS1100Gain>(this->gain_));
  this->parent_->set_sample_rate(static_cast<ADS1100SampleRate>(this->sample_rate_));

  ESP_LOGD(TAG, "Settings propagated to parent component");
  ESP_LOGD(TAG, "Updated parent component settings - Gain: %d, Sample Rate: %d", this->parent_->get_gain(),
           this->parent_->get_sample_rate());
}

void ADS1100Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "ADS1100 Sensor:");
  ESP_LOGCONFIG(TAG, "  Gain: %d", this->gain_);
  ESP_LOGCONFIG(TAG, "  Sample Rate: %d SPS", this->sample_rate_);
  LOG_SENSOR("  ", "Voltage", this);
}

void ADS1100Sensor::update() {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "Parent component not set!");
    this->status_set_error();
    return;
  }

  ESP_LOGD(TAG, "Requesting measurement from parent component");
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