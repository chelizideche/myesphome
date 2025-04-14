#include "xdb401.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace xdb401 {

static const char *const TAG = "xdb401";

static const uint8_t BASE_REGISTER = 0x30;
static const uint8_t READ_COMMAND = 0x0A;

void XDB401Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up XDB401...");

  sint16_t raw_temperature(0);
  sint32_t raw_pressure(0);
  i2c::ErrorCode err_code = this->read_(raw_temperature, raw_pressure);
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
  LOG_SENSOR("  ", "Raw Pressure", this->raw_pressure_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
}

float XDB401Component::get_setup_priority() const { return setup_priority::DATA; }

i2c::ErrorCode XDB401Component::read_(sint16_t &raw_temperature, sint32_t &raw_pressure) {
  const int CHECK_DELAY = 5u;
  const int CHECK_ATTEMPTS = 6u;

  i2c::ErrorCode err_code;

  // initiate data read from device
  err_code = write_register(BASE_REGISTER, &READ_COMMAND, sizeof(READ_COMMAND), true);
  if (err_code != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Error writing config to device, code: %u", err_code);
    this->mark_failed();
    return err_code;
  }
  uint8_t config_resonse[1] = {};
  bool meas_mode = false;
  for (int i = 1; i <= CHECK_ATTEMPTS; i++) {
    delay(CHECK_DELAY);
    err_code = read_register(BASE_REGISTER, config_resonse, sizeof(config_resonse), true);
    if (err_code != i2c::ERROR_OK) {
      ESP_LOGE(TAG, "Error reading config from device, code: %u", err_code);
      this->mark_failed();
      return err_code;
    }
    // Check bit 3 is 0
    if ((config_resonse[0] & 0x08) == 0) {
      meas_mode = true;
      ESP_LOGD(TAG, "Meas mode entered after %u ms", i * CHECK_DELAY);
      break;
    }
  }

  ESP_LOGD(TAG, "Config response %02X", *config_resonse);
  if (!meas_mode) {
    ESP_LOGE(TAG, "Device not in meas mode after timeout of %ums", CHECK_DELAY * CHECK_ATTEMPTS);
    return i2c::ERROR_TIMEOUT;
  }

  // read 3 bytes from senesor at address 0x06
  uint8_t p_data[3] = {0x00, 0x00, 0x00};
  err_code = this->read_register(0x06, p_data, 3, true);
  if (err_code != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Error reading pressure register");
    return err_code;
  }
  ESP_LOGD(TAG, "Got pressure data: %s", format_hex_pretty(p_data, 3).c_str());
  // Byte-order high to low, byte 0 bit 8 is sign bit.
  raw_pressure = ((p_data[0] & 0x7F) << 16 | p_data[1] << 8 | p_data[2]);
  if ((p_data[0] & 0x80) != 0) {
    raw_pressure -= 0x01000000;
  }

  // read 2 bytes from senesor at address 0x09
  uint8_t t_data[2] = {0x00, 0x00};
  err_code = this->read_register(0x09, t_data, 2, true);
  if (err_code != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Error reading temperature register");
    return err_code;
  }
  ESP_LOGD(TAG, "Got temperature data: %s", format_hex_pretty(t_data, 2).c_str());
  // Byte-order high to low, byte 0 bit 8 is sign bit.
  raw_temperature = ((t_data[0] & 0x7F) << 8 | t_data[1]);
  if ((t_data[0] & 0x80) != 0) {
    raw_temperature -= 0x010000;
  }

  return i2c::ERROR_OK;
}

/* Got this code from manufacturer, but have some doubts
   about details in it.*/
// inline void fake_func()
// {
//   uint8_t Pressure[3];
//   uint8_t Temp[2];
//   char data[3];
//   float Cal_PData1; // 24-bit AD value - pressure data
//   float Cal_PData2; // Current pressure as a percentage of full-scale pressure
//   float Pressure_data; // Current actual pressure output
//   float Cal_TData1; // 24-bit AD value - temperature data
//   float Cal_TData2; // Current temperature as a percentage of full-scale temperature
//   float Temp_data; // Current actual temperature output
//   float Fullscale_P; // Full-scale pressure value
//   Fullscale_P = 1000; // Please define the full-scale pressure (e.g., 1000 kPa)

//   while (1)
//   {

//     I2C_WriteReg(0x30, 0x0A);
//     delay_ms(100);
//     I2C_ReadNByte(0x06, Pressure, 3); // Read registers 0x06, 0x07, and 0x08 sequentially. data[0] = Pressure[0];
//     data[1] = Pressure[1];
//     data[2] = Pressure[2];
//     Cal_PData1 = data[0] * 65535 + data[1] * 256 + data[2];
//     if (Cal_PData1 > 8388608)
//     {
//       Cal_PData2 = (Cal_PData1 - 16777216) / 8388608;
//     }
//     else
//     {
//       Cal_PData2 = Cal_PData1 / 8388608;
//     }
//     Pressure_data = Cal_PData2 * Fullscale_P;
//     // Calculate pressure value

//     I2C_ReadNByte(0x09, Temp, 2); // Read registers 0x09 and 0x0A sequentially. data[0] = Temp[0];
//     data[1] = Temp[1];

//     Cal_TData1 = data[0] * 256 + data[1];
//     if (Cal_TData1 > 32768)
//     {
//       Cal_TData2 = (Cal_TData1 - 65536) / 256;
//     }
//     else
//     {
//       Cal_TData2 = Cal_TData1 / 256;
//     }
//     Temp_data = Cal_TData2; // Calculate temperature value

//     // Use Pressure_data and Temp_data as needed in your application
//   }
// }

void XDB401Component::update() {
  sint16_t raw_temperature(0);
  sint32_t raw_pressure(0);

  i2c::ErrorCode err = this->read_(raw_temperature, raw_pressure);

  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C Communication Failed");
    this->status_set_warning();
    return;
  }

  float pressure = (float) raw_pressure / (float) 0x80000 * 1000.0;
  float temperature = (float) raw_temperature / (float) 0x0100;

  ESP_LOGD(TAG, "Got pressure=%.1f, temperature=%.1f°C", pressure, temperature);

  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature);
  if (this->raw_pressure_sensor_ != nullptr)
    this->raw_pressure_sensor_->publish_state(pressure);

  this->status_clear_warning();
}

}  // namespace xdb401
}  // namespace esphome
