#include "s300.h"
#include "esphome/core/log.h"

namespace esphome {
namespace s300 {

static const char *const TAG = "s300";

static const uint8_t S300_COMMAND_READ_DATA[] = {0x52};

// Time after writing command before reading i2c data (minimum is 10ms per S300 I2C programming guide)
static const uint16_t S300_I2C_RESPONSE_WAIT_MS = 10;

float S300Component::read_co2_data_() {
  uint8_t i2c_out_data[7];
  if (this->read(i2c_out_data, 7) != i2c::ERROR_OK) {
    static const char *const ERROR_MSG = "Couldn't read CO2 data via I2C";
    ESP_LOGE(TAG, "%s", ERROR_MSG);
    this->status_set_warning(ERROR_MSG);
    return NAN;
  }

  this->status_clear_warning();

  // Combine lower & upper PPM bytes of CO2 data
  return (float) (i2c_out_data[1] << 8) + (float) i2c_out_data[2];
}

bool S300Component::start_command_(const uint8_t *command_byte) {
  const auto i2c_write_err = this->write(command_byte, 1);
  if (i2c_write_err != i2c::ERROR_OK) {
    static const char *const ERROR_MSG = "Couldn't start measurement via I2C";
    ESP_LOGE(TAG, "%s, error #%d", ERROR_MSG, i2c_write_err);
    this->status_set_warning(ERROR_MSG);
    return false;
  }

  return true;
}

void S300Component::setup() {
  // Dummy read command, will set warning if there's an i2c issue
  this->start_command_(S300_COMMAND_READ_DATA);
}

void S300Component::update() {
  ESP_LOGV(TAG, "Update '%s' S300 CO2 Sensor", this->name_.c_str());

  if (this->start_command_(S300_COMMAND_READ_DATA)) {
    this->set_timeout(S300_I2C_RESPONSE_WAIT_MS, [this]() {
      const float co2_val = this->read_co2_data_();
      this->publish_state(co2_val);
    });
  }
}

void S300Component::dump_config() {
  LOG_SENSOR("", "S300", this);
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace s300
}  // namespace esphome
