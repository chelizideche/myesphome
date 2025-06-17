#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace sht20 {

class SHT20Component : public PollingComponent, public i2c::I2CDevice
{
public:
  void setup() override;
  void update() override;

  void dump_config() override;

  float get_setup_priority() const override { return esphome::setup_priority::DATA; }

  void set_temperature_sensor( sensor::Sensor * temperature_sensor ) { this->temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor( sensor::Sensor * humidity_sensor ) { this->humidity_sensor_ = humidity_sensor; }

protected:
  sensor::Sensor *temperature_sensor_ { nullptr };
  sensor::Sensor *humidity_sensor_ { nullptr };

  bool send_command_( uint8_t cmd );
  bool force_otp_reload_();

  void advance_measurements_();

  uint8_t measurement_progress_ { 0 };
  uint8_t measurement_results_ [ 4 ];
};

} // namespace sht20
} // namespace esphome

