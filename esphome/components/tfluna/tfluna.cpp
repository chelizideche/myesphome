#include "tfluna.h"
#include <cstddef>
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace tfluna {

// see https://files.waveshare.com/upload/a/ac/SJ-PM-TF-Luna_A05_Product_Manual.pdf
static const uint8_t VERSION_REVISION_REGISTER = 0x0A;
static const uint8_t VERSION_MINOR_REGISTER = 0x0B;
static const uint8_t VERSION_MAJOR_REGISTER = 0x0C;
static const uint8_t AMP_LOW_REGISTER = 0x02;
static const uint8_t AMP_HIGH_REGISTER = 0x03;
static const uint8_t TEMPERATURE_LOW_REGISTER = 0x04;
static const uint8_t TEMPERATURE_HIGH_REGISTER = 0x05;
static const uint8_t DISTANCE_LOW_REGISTER = 0x00;
static const uint8_t DISTANCE_HIGH_REGISTER = 0x01;
static const uint8_t MODE_REGISTER = 0x23;
static const uint8_t MODE_CONTINUOUS = 0x00;
static const uint8_t MODE_TRIGGER = 0x01;
static const uint8_t TRIGGER_ONESHOT_REGISTER = 0x24;
static const char *const TAG = "tfluna";

void TFLuna::dump_config() {
  ESP_LOGCONFIG(TAG, "TF-Luna (i2c):");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);

#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Distance:", this->distance_sensor_);
  LOG_SENSOR("  ", "Temperature:", this->temperature_sensor_);
  LOG_SENSOR("  ", "Signal Strength:", this->signal_strength_sensor_);
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Version", this->version_text_sensor_);
#endif
}

void TFLuna::setup() {
  ESP_LOGI(TAG, "Setting up TFLuna...");
  uint8_t major;
  if (!this->read_byte(VERSION_MAJOR_REGISTER, &major)) {
    ESP_LOGE(TAG, "Failed to get major firmware version");
    this->mark_failed();
    return;
  }
  uint8_t minor;
  if (!this->read_byte(VERSION_MINOR_REGISTER, &minor)) {
    ESP_LOGE(TAG, "Failed to get minor firmware version");
    this->mark_failed();
    return;
  }
  uint8_t revision;
  if (!this->read_byte(VERSION_REVISION_REGISTER, &revision)) {
    ESP_LOGE(TAG, "Failed to get revision firmware version");
    this->mark_failed();
    return;
  }
  auto version = str_sprintf("%d.%d.%d", major, minor, revision);
  ESP_LOGI(TAG, "TFLuna Firmware: %s", version.c_str());

#ifdef USE_TEXT_SENSOR
  if (this->version_text_sensor_ != nullptr) {
    this->version_text_sensor_->publish_state(version);
  }
#endif

  if (!this->write_byte(MODE_REGISTER, MODE_TRIGGER)) {
    ESP_LOGE(TAG, "Failed to set mode to trigger mode");
    this->mark_failed();
    return;
  }
}

void TFLuna::update() {
  if (!this->write_byte(TRIGGER_ONESHOT_REGISTER, 0x01)) {
    ESP_LOGE(TAG, "Failed to trigger a oneshot");
    this->status_set_warning();
    return;
  }

#ifdef USE_SENSOR
  if (this->distance_sensor_ != nullptr) {
    uint8_t distance_low;
    if (!this->read_byte(DISTANCE_LOW_REGISTER, &distance_low)) {
      ESP_LOGE(TAG, "Failed to get distance low");
      this->status_set_warning();
      return;
    }
    uint8_t distance_high;
    if (!this->read_byte(DISTANCE_HIGH_REGISTER, &distance_high)) {
      ESP_LOGE(TAG, "Failed to get distance high");
      this->status_set_warning();
      return;
    }
    uint16_t distance = distance_low + distance_high * 256;

    ESP_LOGD(TAG, "Got distance=%d cm", distance);
    this->distance_sensor_->publish_state(distance);
  }

  if (this->temperature_sensor_ != nullptr) {
    uint8_t temperature_low;
    if (!this->read_byte(TEMPERATURE_LOW_REGISTER, &temperature_low)) {
      ESP_LOGE(TAG, "Failed to get temperature low");
      this->status_set_warning();
      return;
    }
    uint8_t temperature_high;
    if (!this->read_byte(TEMPERATURE_HIGH_REGISTER, &temperature_high)) {
      ESP_LOGE(TAG, "Failed to get temperature high");
      this->status_set_warning();
      return;
    }
    float temperature = (temperature_low + temperature_high * 256) / (float) 100;

    ESP_LOGD(TAG, "Got temperature=%f degrees celsius", temperature);
    this->temperature_sensor_->publish_state(temperature);
  }

  if (this->signal_strength_sensor_ != nullptr) {
    uint8_t signal_strength_low;
    if (!this->read_byte(AMP_LOW_REGISTER, &signal_strength_low)) {
      ESP_LOGE(TAG, "Failed to get signal strength low");
      this->status_set_warning();
      return;
    }
    uint8_t signal_strength_high;
    if (!this->read_byte(AMP_HIGH_REGISTER, &signal_strength_high)) {
      ESP_LOGE(TAG, "Failed to get signal strength high");
      this->status_set_warning();
      return;
    }
    uint16_t signal_strength = signal_strength_low + signal_strength_high * 256;

    ESP_LOGD(TAG, "Got signal strength=%d", signal_strength);
    this->signal_strength_sensor_->publish_state(signal_strength);
  }
#endif
  this->status_clear_warning();
}

}  // namespace tfluna
}  // namespace esphome
