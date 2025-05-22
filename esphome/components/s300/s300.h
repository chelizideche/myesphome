#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

#include <cinttypes>

namespace esphome {
namespace s300 {

/// This class implements support for ELT Sensor's s300 co2 sensor using i2c.
class S300Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_co2_sensor(sensor::Sensor *co2_sensor) { co2_sensor_ = co2_sensor; }

  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; };
  void dump_config() override;

  float read_co2_data();

 protected:
  bool start_command(const uint8_t *command_byte);
  sensor::Sensor *co2_sensor_{nullptr};
};

}  // namespace s300
}  // namespace esphome
