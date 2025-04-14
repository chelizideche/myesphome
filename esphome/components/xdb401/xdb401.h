#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace xdb401 {

/// This class implements support for the xdb401 pressure and temperature i2c sensors.
class XDB401Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
  void set_raw_pressure_sensor(sensor::Sensor *raw_pressure_sensor) {
    this->raw_pressure_sensor_ = raw_pressure_sensor;
  }

  float get_setup_priority() const override;
  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  i2c::ErrorCode read_(sint16_t &raw_temperature, sint32_t &raw_pressure);
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *raw_pressure_sensor_{nullptr};
};

}  // namespace xdb401
}  // namespace esphome
