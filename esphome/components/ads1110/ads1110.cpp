#include "ads1110.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ads1110 {

static const char *const TAG = "ads1110";
static const uint8_t ADS1110_REGISTER_CONVERSION = 0x00;
static const uint8_t ADS1110_REGISTER_CONFIG = 0x01;

void ADS1110Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADS1110...");
  LOG_I2C_DEVICE(this);

  // Log initial state
  ESP_LOGD(TAG, "Initial state - Gain: %d, Sample Rate: %d", this->gain_, this->sample_rate_);

  // We need a simple, straightforward approach with the ADS1110
  // Success criteria is being able to read the conversion register

  // Initial delay to allow device to power up fully
  delay(200);

  // Most basic check - a simple I2C read of 2 bytes from register 0
  uint16_t value;
  for (int retry = 0; retry < 3; retry++) {
    if (this->read_byte_16(ADS1110_REGISTER_CONVERSION, &value)) {
      ESP_LOGD(TAG, "ADS1110 communication successful, raw value: 0x%04X", value);

      // If we can read data, the device is working - mark it as initialized
      this->i2c_initialized_ = true;
      ESP_LOGCONFIG(TAG, "ADS1110 initialized successfully!");
      return;
    }
    ESP_LOGW(TAG, "ADS1110 communication failed, retry %d", retry + 1);
    delay(50);
  }

  // If we get here, we couldn't read the device after multiple attempts
  ESP_LOGE(TAG, "Communication with ADS1110 failed!");
  this->mark_failed();
}

void ADS1110Component::dump_config() {
  ESP_LOGCONFIG(TAG, "ADS1110:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with ADS1110 failed!");
  }
  ESP_LOGCONFIG(TAG, "  Gain: %d", this->gain_);
  ESP_LOGCONFIG(TAG, "  Sample Rate: %d SPS",
                this->sample_rate_ == ADS1110_SAMPLE_RATE_128_SPS  ? 128
                : this->sample_rate_ == ADS1110_SAMPLE_RATE_32_SPS ? 32
                : this->sample_rate_ == ADS1110_SAMPLE_RATE_16_SPS ? 16
                                                                   : 8);
}

float ADS1110Component::request_measurement() {
  if (!this->i2c_initialized_) {
    ESP_LOGE(TAG, "ADS1110 not initialized");
    return NAN;
  }

  // For ADS1110, we just read the conversion register
  // It operates in continuous conversion mode by default
  uint16_t raw_adc;
  if (!this->read_byte_16(ADS1110_REGISTER_CONVERSION, &raw_adc)) {
    ESP_LOGE(TAG, "Failed to read ADC value");
    return NAN;
  }

  ESP_LOGVV(TAG, "Raw ADC value: 0x%04X", raw_adc);

  // Convert raw ADC value to voltage
  // The ADS1110 returns a signed 16-bit value
  int16_t value = raw_adc;

  // For ADS1110, according to datasheet:
  // Gain=1: ±2.048V
  // Gain=2: ±1.024V
  // Gain=4: ±0.512V
  // Gain=8: ±0.256V
  float voltage;
  switch (this->gain_) {
    case ADS1110_GAIN_1:
      voltage = value * 2.048f / 32768.0f;
      break;
    case ADS1110_GAIN_2:
      voltage = value * 1.024f / 32768.0f;
      break;
    case ADS1110_GAIN_4:
      voltage = value * 0.512f / 32768.0f;
      break;
    case ADS1110_GAIN_8:
      voltage = value * 0.256f / 32768.0f;
      break;
    default:
      ESP_LOGE(TAG, "Invalid gain setting: %d", this->gain_);
      return NAN;
  }

  ESP_LOGD(TAG, "Raw ADC: %d, Voltage: %.4f V", value, voltage);
  return voltage;
}

}  // namespace ads1110
}  // namespace esphome