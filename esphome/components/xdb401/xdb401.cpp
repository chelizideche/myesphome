#include "xdb401.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace xdb401 {

static const char *const TAG = "xdb401";

static const uint8_t REG_PRESSURE = 0x06;
static const uint8_t REG_TEMPERATURE = 0x09;
static const uint8_t REG_MAKE_MEASURE = 0x30;
static const uint8_t CMD_MAKE_MEASURE = 0x0A;
static const uint8_t MASK_MEASURE_READY = 0x08;
static const float CONVERT_PRESSURE = (float) 0x800000;
static const float SCALE_PRESSURE = 1.0e6;
static const float SCALE_TEMPERATURE = 100.0;

static const int CHECK_DELAY = 5u;
static const int CHECK_ATTEMPTS = 6u;

void XDB401Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up XDB401...");

  float temperature;
  float pressure;
  i2c::ErrorCode err_code = this->read_(temperature, pressure);
  if (err_code != i2c::ERROR_OK) {
    ESP_LOGCONFIG(TAG, "    I2C Communication Failed...");
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "    Success...");
}

void XDB401Component::dump_config() {
  ESP_LOGCONFIG(TAG, "XDB401:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
}

float XDB401Component::get_setup_priority() const { return setup_priority::DATA; }

i2c::ErrorCode XDB401Component::read_(float &temperature, float &pressure) {
  i2c::ErrorCode err_code;

  err_code = this->set_meas_mode_();
  if (err_code != i2c::ERROR_OK) {
    this->mark_failed();
    return err_code;
  }

  err_code = this->read_pressure_(pressure);
  if (err_code != i2c::ERROR_OK) {
    this->mark_failed();
    return err_code;
  }

  err_code = this->read_temperature_(temperature);
  if (err_code != i2c::ERROR_OK) {
    this->mark_failed();
    return err_code;
  }

  return i2c::ERROR_OK;
}

i2c::ErrorCode XDB401Component::set_meas_mode_() {
  i2c::ErrorCode err_code = i2c::ERROR_OK;

  // Initiate data read from device
  err_code = write_register(REG_MAKE_MEASURE, &CMD_MAKE_MEASURE, sizeof(CMD_MAKE_MEASURE), true);
  if (err_code != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Error writing config to device, code: %u", err_code);
    this->mark_failed();
    return err_code;
  }

  // Wait for measurement ready
  uint8_t meas_resp[1] = {};
  bool meas_mode = false;
  for (int i = 1; i <= CHECK_ATTEMPTS; i++) {
    delay(CHECK_DELAY);
    err_code = read_register(REG_MAKE_MEASURE, meas_resp, sizeof(meas_resp), true);
    if (err_code != i2c::ERROR_OK) {
      ESP_LOGE(TAG, "Error reading config from device, code: %u", err_code);
      this->mark_failed();
      return err_code;
    }
    // Bit 3 shall be 0
    if ((meas_resp[0] & MASK_MEASURE_READY) == 0) {
      meas_mode = true;
      ESP_LOGV(TAG, "Meas mode entered after %u ms", i * CHECK_DELAY);
      break;
    }
  }

  ESP_LOGV(TAG, "Config response %02X", *meas_resp);
  if (!meas_mode) {
    ESP_LOGE(TAG, "Device not in measurement mode after timeout of %ums", CHECK_DELAY * CHECK_ATTEMPTS);
    return i2c::ERROR_TIMEOUT;
  }

  return err_code;
}

i2c::ErrorCode XDB401Component::read_pressure_(float &pressure) {
  i2c::ErrorCode err_code = i2c::ERROR_OK;

  // Read 3 bytes from senesor at address 0x06
  uint8_t p_data[3]{};
  err_code = this->read_register(REG_PRESSURE, p_data, 3, true);
  if (err_code != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Error reading pressure register");
    return err_code;
  }
  ESP_LOGV(TAG, "Got pressure data: %s", format_hex_pretty(p_data, 3).c_str());

  // Byte-order high to low, byte 0 bit 8 is sign bit.
  // Shift one byte to much and then back to get sign correct.
  sint32_t raw_pressure = ((p_data[0] << 24) | (p_data[1] << 16) | (p_data[2] << 8)) >> 8;
  ESP_LOGD(TAG, "Pressure data raw %i", raw_pressure);

  // Convert signed integer to floating point and scale to Pascal
  pressure = (float) raw_pressure / CONVERT_PRESSURE * SCALE_PRESSURE;

  return err_code;
}

i2c::ErrorCode XDB401Component::read_temperature_(float &temperature) {
  i2c::ErrorCode err_code = i2c::ERROR_OK;

  // Read 2 bytes from senesor at address 0x09
  uint8_t t_data[2]{};
  err_code = this->read_register(REG_TEMPERATURE, t_data, 2, true);
  if (err_code != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Error reading temperature register");
    return err_code;
  }
  ESP_LOGV(TAG, "Got temperature data: %s", format_hex_pretty(t_data, 2).c_str());

  // Byte-order high to low, byte 0 bit 8 is sign bit.
  sint16_t raw_temperature = ((t_data[0] << 8) | t_data[1]);
  ESP_LOGD(TAG, "Temperature data raw %i", raw_temperature);

  // Convert signed integer to floating point and scale to percent (of range)?
  // temperature = (float) raw_temperature / (float) 0x100;
  // Convert signed integer to floating point and scale to Celcius
  temperature = (float) raw_temperature / SCALE_TEMPERATURE;

  return err_code;
}

void XDB401Component::update() {
  float temperature{};
  float pressure{};

  i2c::ErrorCode err = this->read_(temperature, pressure);

  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C Communication Failed");
    this->status_set_warning();
    return;
  }

  ESP_LOGD(TAG, "Got pressure=%.1f, temperature=%.1f°C", pressure, temperature);

  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature);
  if (this->pressure_sensor_ != nullptr)
    this->pressure_sensor_->publish_state(pressure);

  this->status_clear_warning();
}

}  // namespace xdb401
}  // namespace esphome
