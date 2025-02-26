#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "gl_r01_i2c.h"

namespace esphome {
namespace gl_r01_i2c {

static const char *const TAG = "gl_r01_i2c";

// Register definitions from datasheet
static const uint8_t REG_VERSION = 0x00;
static const uint8_t REG_DISTANCE = 0x02;
static const uint8_t REG_TRIGGER = 0x10;
static const uint8_t CMD_TRIGGER = 0xB0;
static const uint8_t RESTART_CMD1 = 0x5A;
static const uint8_t RESTART_CMD2 = 0xA5;

void GLR01I2CComponent::set_min_read_interval(uint32_t min_read_interval) {
  this->min_read_interval_ = min_read_interval;
}

void GLR01I2CComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up GL-R01 I2C...");
  // Verify sensor presence
  if (!this->read_byte_16(REG_VERSION, &this->version_)) {
    ESP_LOGE(TAG, "Failed to communicate with GL-R01 I2C sensor!");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Found GL-R01 I2C with version 0x%04X", this->version_);
}

void GLR01I2CComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "GL-R01 I2C:");
  ESP_LOGCONFIG(TAG, " Firmware Version: 0x%04X", this->version_);
  ESP_LOGCONFIG(TAG, " Minimum read interval: %u ms", this->min_read_interval_);
  LOG_I2C_DEVICE(this);
  LOG_SENSOR(" ", "Distance", this);
}

void GLR01I2CComponent::update() {
  if (this->is_failed())
    return;
  if (state_ != GLR01I2CState::IDLE) {
    ESP_LOGVV(TAG, "Previous measurement still in progress");
    return;
  }

  // Trigger a new measurement
  if (!this->write_byte(REG_TRIGGER, CMD_TRIGGER)) {
    ESP_LOGE(TAG, "Failed to trigger measurement!");
    this->status_set_warning();
    return;
  }

  state_ = GLR01I2CState::TRIGGERED;
  trigger_time_ = millis();
}

void GLR01I2CComponent::loop() {
  if (this->is_failed())
    return;
  // Wait for result after measurement was triggered
  if (state_ == GLR01I2CState::TRIGGERED) {
    if (millis() - trigger_time_ >= min_read_interval_) {
      state_ = GLR01I2CState::READY;
    }
  }

  if (state_ == GLR01I2CState::READY) {
    uint16_t distance = 0;
    if (!this->read_byte_16(REG_DISTANCE, &distance)) {
      ESP_LOGE(TAG, "Failed to read distance value!");
      this->status_set_warning();
      state_ = GLR01I2CState::IDLE;
      return;
    }

    if (distance == 0xFFFF) {
      ESP_LOGW(TAG,
               "Invalid measurement received! Check connection or consider increasing min_read_interval from %u ms!",
               this->min_read_interval_);
      this->status_set_warning();
    } else {
      ESP_LOGV(TAG, "Distance: %umm", distance);
      this->publish_state(distance);
      this->status_clear_warning();
    }

    state_ = GLR01I2CState::IDLE;
  }
}

}  // namespace gl_r01_i2c
}  // namespace esphome
