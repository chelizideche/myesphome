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
    static const char *const message = "Couldn't read CO2 data via I2C";
    ESP_LOGE(TAG, "%s", message);
    this->status_set_warning(message);
    return NAN;
  }

  this->status_clear_warning();

  // Combine lower & upper PPM bytes of CO2 data
  return (float) (i2c_out_data[1] << 8) + (float) i2c_out_data[2];
}

bool S300Component::start_command_(const uint8_t *command_byte) {
  if (this->write(command_byte, 1) != i2c::ERROR_OK) {
    static const char *const message = "Couldn't start measurement via I2C";
    ESP_LOGE(TAG, "%s", message);
    this->status_set_warning(message);
    return false;
  }

  return true;
}

void S300Component::setup() {
  const auto err = this->write(nullptr, 0);
  if (err != i2c::ERROR_OK) {
    this->mark_failed(ESP_LOG_MSG_COMM_FAIL);
    return;
  }
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
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL_FOR, this->name_.c_str());
  }
}

}  // namespace s300
}  // namespace esphome
