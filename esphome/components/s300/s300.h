#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

#include <cinttypes>

namespace esphome {
namespace s300 {

/// This class implements support for ELT Sensor's s300 co2 sensor using i2c.
class S300Component : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; };
  void dump_config() override;

 protected:
  bool start_command_(const uint8_t *command_byte);
  float read_co2_data_();
};

}  // namespace s300
}  // namespace esphome
