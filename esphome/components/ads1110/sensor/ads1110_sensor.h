#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/hal.h"
#include "../ads1110.h"

namespace esphome {
namespace ads1110 {

class ADS1110Sensor : public sensor::Sensor, public PollingComponent {
 public:
  void set_parent(ADS1110Component *parent) { parent_ = parent; }
  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  ADS1110Component *parent_;
};

}  // namespace ads1110
}  // namespace esphome