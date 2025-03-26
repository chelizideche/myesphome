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
  ESP_LOGD(TAG, "Attempting to initialize I2C communication...");
  uint8_t test_byte;
  if (!this->read_byte(0x00, &test_byte)) {
    ESP_LOGE(TAG, "Failed to initialize I2C communication:");
    ESP_LOGE(TAG, "  - Device not responding at address 0x%02X", this->address_);
    ESP_LOGE(TAG, "  - Check if device is properly connected");
    ESP_LOGE(TAG, "  - Verify I2C bus is properly configured");
    ESP_LOGE(TAG, "  - Check if pull-up resistors are present");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Initial I2C read successful, test byte: 0x%02X", test_byte);
  this->i2c_initialized_ = true;
  delay(10);  // Small delay after initialization

  // Configure the device with a fresh configuration
  uint16_t config = 0;

  // Set single-shot mode (bit 15)
  config |= 0x8000;  // ADS1100_REG_CONFIG_OS_SINGLE

  // Set continuous mode (bit 8)
  config &= ~0x0100;  // ADS1100_REG_CONFIG_MODE_CONTIN

  // Set sample rate (bits 2-3)
  config |= (this->sample_rate_ & 0x03) << 2;

  // Set gain (bits 0-1)
  config |= (this->gain_ & 0x03);

  ESP_LOGD(TAG, "Writing initial config: 0x%04X", config);
  ESP_LOGD(TAG, "Config bits:");
  ESP_LOGD(TAG, "  Single-shot: %d", (config >> 15) & 0x01);
  ESP_LOGD(TAG, "  Sample Rate: %d", (config >> 2) & 0x03);
  ESP_LOGD(TAG, "  Gain: %d", config & 0x03);

  // Write new config
  if (!this->write_byte_16(ADS1100_REGISTER_CONFIG, config)) {
    ESP_LOGE(TAG, "Failed to write config register:");
    ESP_LOGE(TAG, "  - I2C write error at register 0x%02X", ADS1100_REGISTER_CONFIG);
    ESP_LOGE(TAG, "  - Attempted to write value: 0x%04X", config);
    ESP_LOGE(TAG, "  - Device may be locked or in an invalid state");
    ESP_LOGE(TAG, "  - Check if device is properly powered");
    ESP_LOGE(TAG, "  - Verify I2C clock speed is not too high");
    this->mark_failed();
    return;
  }
  delay(10);  // Small delay after write
  this->prev_config_ = config;

  // Verify the config was written correctly
  uint16_t read_config;
  ESP_LOGD(TAG, "Verifying config write...");
  if (!this->read_byte_16(ADS1100_REGISTER_CONFIG, &read_config)) {
    ESP_LOGE(TAG, "Failed to read back config register:");
    ESP_LOGE(TAG, "  - I2C read error at register 0x%02X", ADS1100_REGISTER_CONFIG);
    ESP_LOGE(TAG, "  - Device may be in an invalid state");
    ESP_LOGE(TAG, "  - Check if device is properly powered");
    ESP_LOGE(TAG, "  - Verify I2C clock speed is not too high");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Read back config: 0x%04X", read_config);
  ESP_LOGD(TAG, "Read back config bits:");
  ESP_LOGD(TAG, "  Single-shot: %d", (read_config >> 15) & 0x01);
  ESP_LOGD(TAG, "  Sample Rate: %d", (read_config >> 2) & 0x03);
  ESP_LOGD(TAG, "  Gain: %d", read_config & 0x03);

  if (read_config != config) {
    ESP_LOGE(TAG, "Config verification failed:");
    ESP_LOGE(TAG, "  - Expected config: 0x%04X", config);
    ESP_LOGE(TAG, "  - Read back config: 0x%04X", read_config);
    ESP_LOGE(TAG, "  - Bit differences:");
    ESP_LOGE(TAG, "    * Single-shot: Expected %d, Got %d", (config >> 15) & 0x01, (read_config >> 15) & 0x01);
    ESP_LOGE(TAG, "    * Sample Rate: Expected %d, Got %d", (config >> 2) & 0x03, (read_config >> 2) & 0x03);
    ESP_LOGE(TAG, "    * Gain: Expected %d, Got %d", config & 0x03, read_config & 0x03);
    ESP_LOGE(TAG, "  - Device may be malfunctioning or in an invalid state");
    ESP_LOGE(TAG, "  - Check if device is properly powered");
    ESP_LOGE(TAG, "  - Verify I2C clock speed is not too high");
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
  if (!this->i2c_initialized_) {
    ESP_LOGE(TAG, "Cannot request measurement - I2C not initialized");
    return NAN;
  }

  // Start a new conversion
  uint16_t config = this->prev_config_;
  config |= 0x8000;  // Set single-shot mode (ADS1100_REG_CONFIG_OS_SINGLE)
  ESP_LOGD(TAG, "Starting conversion with config: 0x%04X", config);
  ESP_LOGD(TAG, "Config bits:");
  ESP_LOGD(TAG, "  Single-shot: %d", (config >> 15) & 0x01);
  ESP_LOGD(TAG, "  Sample Rate: %d", (config >> 2) & 0x03);
  ESP_LOGD(TAG, "  Gain: %d", config & 0x03);

  if (!this->write_byte_16(ADS1100_REGISTER_CONFIG, config)) {
    ESP_LOGE(TAG, "Failed to write config for conversion:");
    ESP_LOGE(TAG, "  - I2C write error at register 0x%02X", ADS1100_REGISTER_CONFIG);
    ESP_LOGE(TAG, "  - Attempted to write value: 0x%04X", config);
    ESP_LOGE(TAG, "  - Device may be locked or in an invalid state");
    ESP_LOGE(TAG, "  - Check if device is properly powered");
    ESP_LOGE(TAG, "  - Verify I2C clock speed is not too high");
    return NAN;
  }

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

  // Read the conversion result
  uint16_t raw_value;
  if (!this->read_byte_16(ADS1100_REGISTER_CONVERSION, &raw_value)) {
    ESP_LOGE(TAG, "Failed to read conversion result:");
    ESP_LOGE(TAG, "  - I2C read error at register 0x%02X", ADS1100_REGISTER_CONVERSION);
    ESP_LOGE(TAG, "  - Device may be in an invalid state");
    ESP_LOGE(TAG, "  - Check if device is properly powered");
    ESP_LOGE(TAG, "  - Verify I2C clock speed is not too high");
    return NAN;
  }

  // Convert to voltage based on gain setting
  float voltage;
  switch (this->gain_) {
    case ADS1100_GAIN_1:
      voltage = (raw_value * 1.0f) / 32768.0f;
      break;
    case ADS1100_GAIN_2:
      voltage = (raw_value * 2.0f) / 32768.0f;
      break;
    case ADS1100_GAIN_4:
      voltage = (raw_value * 4.0f) / 32768.0f;
      break;
    case ADS1100_GAIN_8:
      voltage = (raw_value * 8.0f) / 32768.0f;
      break;
    default:
      ESP_LOGE(TAG, "Invalid gain setting: %d", this->gain_);
      return NAN;
  }

  ESP_LOGD(TAG, "Raw value: %d, Voltage: %.3f V", raw_value, voltage);
  return voltage;
}

}  // namespace ads1100
}  // namespace esphome