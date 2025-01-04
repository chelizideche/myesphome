#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace lc709203f {

class lc709203f : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  
  void set_pack_size(uint16_t PackSize);
  void set_thermistor_B_constant(uint16_t B_Constant);
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_battery_remaining_sensor(sensor::Sensor *battery_remaining_sensor) { battery_remaining_sensor_ = battery_remaining_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  
  //TODO: Move these to protected? they should probably be private.
  uint8_t GetRegister(uint8_t RegisterToRead, uint16_t *RegisterValue);
  uint8_t SetRegister(uint8_t RegisterToSet, uint16_t ValueToSet);
  uint8_t CRC8 (uint8_t *ByteBuffer, uint8_t LengthOfCRC);
  
  protected:
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *battery_remaining_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  uint16_t pack_size_;
  uint16_t APA_;
  uint16_t B_Constant_;
  uint8_t State_;
  
};

}  // namespace lc709203f
}  // namespace esphome
