#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace gl_r01_i2c {

enum class GLR01I2CState { IDLE, TRIGGERED, READY };

class GLR01I2CComponent : public sensor::Sensor, public i2c::I2CDevice, public PollingComponent {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  bool restart_sensor();

  void set_min_read_interval(uint32_t min_read_interval);

 protected:
  GLR01I2CState state_{GLR01I2CState::IDLE};
  uint32_t trigger_time_{0};
  uint32_t min_read_interval_{40};  // minimum milliseconds from datasheet
  uint16_t version_{0};
};

class RestartSensorAction : public Action<> {
 public:
  explicit RestartSensorAction(GLR01I2CComponent *parent) : parent_(parent) {}
  void play() override;

 protected:
  GLR01I2CComponent *parent_;
};

}  // namespace gl_r01_i2c
}  // namespace esphome
