#include "s300.h"
#include "esphome/core/log.h"

namespace esphome {
namespace s300 {

static const char *const TAG = "s300";

static const uint8_t S300_START_TRANSMISSION[] = {0x62};
static const uint8_t S300_COMMAND_READ_DATA[] = {0x52};

float S300Component::read_co2_data() {
  uint8_t i2c_out_data[7];
  if (this->read(i2c_out_data, 7) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to read CO2 data from S300!");
    this->status_set_warning("couldn't read i2c command response");
    return 0;
  }

  this->status_clear_warning();

  // Combine lower & upper PPM bytes of CO2 data
  return (float) (i2c_out_data[1] << 8) + (float) i2c_out_data[2];
}

bool S300Component::start_command(const uint8_t *command_byte) {
  if (this->write(command_byte, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to send command to S300!");
    this->status_set_warning("couldn't start measurement");
    return false;
  }

  return true;
}

void S300Component::update() {
  ESP_LOGV(TAG, "Update S300 CO2 Sensor");

  if (this->start_command(S300_COMMAND_READ_DATA)) {
    // Wait before reading i2c data to give sensor time to process the command (minimum is 10ms per I2C programming
    // guide)
    this->set_timeout(20, [this]() {
      const float co2_val = this->read_co2_data();
      if (this->status_has_warning())
        return;

      if (this->co2_sensor_ != nullptr)
        this->co2_sensor_->publish_state(co2_val);
    });
  }
}

void S300Component::dump_config() {
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace s300
}  // namespace esphome
