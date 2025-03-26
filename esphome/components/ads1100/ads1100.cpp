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
  LOG_I2C_DEVICE(this);

  // Log initial state
  ESP_LOGD(TAG, "Initial state - Gain: %d, Sample Rate: %d", this->gain_, this->sample_rate_);
  ESP_LOGD(TAG, "I2C Address: 0x%02X", this->address_);

  // Initialize I2C
  if (!this->read_byte(0x00)) {
    ESP_LOGE(TAG, "Failed to initialize I2C communication");
    this->mark_failed();
    return;
  }
  this->i2c_initialized_ = true;
  ESP_LOGD(TAG, "I2C communication initialized successfully");

  // First try to read the conversion register to verify communication
  uint16_t value;
  ESP_LOGD(TAG, "Attempting to read conversion register...");
  if (!this->read_byte_16(ADS1100_REGISTER_CONVERSION, &value)) {
    ESP_LOGE(TAG, "Failed to read conversion register - I2C error");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Initial conversion value: 0x%04X", value);
  delay(10);  // Small delay after read

  // Read current config
  uint16_t current_config;
  ESP_LOGD(TAG, "Attempting to read config register...");
  if (!this->read_byte_16(ADS1100_REGISTER_CONFIG, &current_config)) {
    ESP_LOGE(TAG, "Failed to read config register - I2C error");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Current config: 0x%04X", current_config);
  delay(10);  // Small delay after read

  // Configure the device
  uint16_t config = 0;
  // Clear single-shot bit
  //        0b0xxxxxxxxxxxxxxx
  config |= 0b0000000000000000;

  // Set continuous mode
  //        0bxxxxxxx0xxxxxxxx
  config |= 0b0000000000000000;

  // Set sample rate
  //        0bxxxxxxxxBBxxxxxx
  config |= (this->sample_rate_ & 0b11) << 2;

  // Set gain
  //        0bxxxxxxxxxxBBxxxx
  config |= (this->gain_ & 0b11);

  ESP_LOGD(TAG, "Writing config: 0x%04X (gain: %d, sample_rate: %d)", config, this->gain_, this->sample_rate_);

  // Write new config
  if (!this->write_byte_16(ADS1100_REGISTER_CONFIG, config)) {
    ESP_LOGE(TAG, "Failed to write config register - I2C error");
    this->mark_failed();
    return;
  }
  delay(10);  // Small delay after write
  this->prev_config_ = config;

  // Verify the config was written correctly
  uint16_t read_config;
  ESP_LOGD(TAG, "Verifying config write...");
  if (!this->read_byte_16(ADS1100_REGISTER_CONFIG, &read_config)) {
    ESP_LOGE(TAG, "Failed to read back config register - I2C error");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Read back config: 0x%04X", read_config);

  if (read_config != config) {
    ESP_LOGE(TAG, "Config verification failed. Expected: 0x%04X, Got: 0x%04X", config, read_config);
    this->mark_failed();
    return;
  }

  ESP_LOGD(TAG, "ADS1100 setup completed successfully");
}

void ADS1100Component::dump_config() {
  ESP_LOGCONFIG(TAG, "ADS1100:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with ADS1100 failed!");
  }
  ESP_LOGCONFIG(TAG, "  Gain: %d", this->gain_);
  ESP_LOGCONFIG(TAG, "  Sample Rate: %d SPS",
                this->sample_rate_ == ADS1100_SAMPLE_RATE_128_SPS  ? 128
                : this->sample_rate_ == ADS1100_SAMPLE_RATE_32_SPS ? 32
                : this->sample_rate_ == ADS1100_SAMPLE_RATE_16_SPS ? 16
                                                                   : 8);
}

float ADS1100Component::request_measurement() {
  uint16_t config = this->prev_config_;

  // Start conversion
  config |= 0b1000000000000000;

  ESP_LOGD(TAG, "Starting conversion with config: 0x%04X", config);
  if (!this->write_byte_16(ADS1100_REGISTER_CONFIG, config)) {
    ESP_LOGE(TAG, "Failed to write config for conversion");
    this->status_set_warning();
    return NAN;
  }
  delay(10);  // Small delay after write
  this->prev_config_ = config;

  // Wait for conversion to complete based on sample rate
  int delay_ms;
  switch (this->sample_rate_) {
    case ADS1100_SAMPLE_RATE_8_SPS:
      delay_ms = 126;  // NOLINT
      break;
    case ADS1100_SAMPLE_RATE_16_SPS:
      delay_ms = 63;  // NOLINT
      break;
    case ADS1100_SAMPLE_RATE_32_SPS:
      delay_ms = 32;
      break;
    case ADS1100_SAMPLE_RATE_128_SPS:
    default:
      delay_ms = 9;
      break;
  }
  ESP_LOGD(TAG, "Waiting %d ms for conversion", delay_ms);
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
    ESP_LOGE(TAG, "Failed to read conversion result");
    this->status_set_warning();
    return NAN;
  }
  ESP_LOGD(TAG, "Raw conversion value: 0x%04X", raw_conversion);

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
  ESP_LOGD(TAG, "Converted voltage: %.3fV", volts);

  this->status_clear_warning();
  return volts;
}

}  // namespace ads1100
}  // namespace esphome