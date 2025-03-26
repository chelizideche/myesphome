#include "ads1100.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ads1100 {

static const char *const TAG = "ads1100";
static const uint8_t ADS1100_REGISTER_CONVERSION = 0x00;
static const uint8_t ADS1100_REGISTER_CONFIG = 0x01;

void ADS1100Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADS1100...");
  uint16_t value;
  if (!this->read_byte_16(ADS1100_REGISTER_CONVERSION, &value)) {
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "Configuring ADS1100...");

  uint16_t config = 0;
  // Clear single-shot bit
  //        0b0xxxxxxxxxxxxxxx
  config |= 0b0000000000000000;

  // Set continuous mode
  //        0bxxxxxxx0xxxxxxxx
  config |= 0b0000000000000000;

  // Set data rate
  //        0bxxxxxxxxBBxxxxxx
  config |= (this->data_rate_ & 0b11) << 2;

  // Set gain
  //        0bxxxxxxxxxxBBxxxx
  config |= (this->gain_ & 0b11);

  if (!this->write_byte_16(ADS1100_REGISTER_CONFIG, config)) {
    this->mark_failed();
    return;
  }
  this->prev_config_ = config;
}

void ADS1100Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Setting up ADS1100...");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with ADS1100 failed!");
  }
  ESP_LOGCONFIG(TAG, "  Gain: %d", this->gain_);
  ESP_LOGCONFIG(TAG, "  Data Rate: %d SPS",
                this->data_rate_ == ADS1100_DATA_RATE_128_SPS  ? 128
                : this->data_rate_ == ADS1100_DATA_RATE_32_SPS ? 32
                : this->data_rate_ == ADS1100_DATA_RATE_16_SPS ? 16
                                                               : 8);
}

float ADS1100Component::request_measurement() {
  uint16_t config = this->prev_config_;

  // Start conversion
  config |= 0b1000000000000000;

  if (!this->write_byte_16(ADS1100_REGISTER_CONFIG, config)) {
    this->status_set_warning();
    return NAN;
  }
  this->prev_config_ = config;

  // Wait for conversion to complete based on data rate
  int delay_ms;
  switch (this->data_rate_) {
    case ADS1100_DATA_RATE_8_SPS:
      delay_ms = 126;  // NOLINT
      break;
    case ADS1100_DATA_RATE_16_SPS:
      delay_ms = 63;  // NOLINT
      break;
    case ADS1100_DATA_RATE_32_SPS:
      delay_ms = 32;
      break;
    case ADS1100_DATA_RATE_128_SPS:
    default:
      delay_ms = 9;
      break;
  }
  delay(delay_ms);

  uint32_t start = millis();
  while (this->read_byte_16(ADS1100_REGISTER_CONFIG, &config) && (config >> 15) == 0) {
    if (millis() - start > 100) {
      ESP_LOGW(TAG, "Reading ADS1100 timed out");
      this->status_set_warning();
      return NAN;
    }
    yield();
  }

  uint16_t raw_conversion;
  if (!this->read_byte_16(ADS1100_REGISTER_CONVERSION, &raw_conversion)) {
    this->status_set_warning();
    return NAN;
  }

  auto signed_conversion = static_cast<int16_t>(raw_conversion);

  // Convert to volts based on gain setting
  float full_scale_voltage;
  switch (this->gain_) {
    case ADS1100_GAIN_1:
      full_scale_voltage = 2.048f;
      break;
    case ADS1100_GAIN_2:
      full_scale_voltage = 1.024f;
      break;
    case ADS1100_GAIN_4:
      full_scale_voltage = 0.512f;
      break;
    case ADS1100_GAIN_8:
      full_scale_voltage = 0.256f;
      break;
    default:
      full_scale_voltage = 2.048f;
  }
  float volts = (signed_conversion * full_scale_voltage) / 32768.0f;

  this->status_clear_warning();
  return volts;
}

}  // namespace ads1100
}  // namespace esphome