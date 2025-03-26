#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/hal.h"
#include "../ads1100.h"

namespace esphome {
namespace ads1100 {

class ADS1100Sensor : public sensor::Sensor, public PollingComponent {
 public:
  void set_parent(ADS1100Component *parent) { parent_ = parent; }
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_gain(uint8_t gain) { this->gain_ = gain; }
  void set_sample_rate(uint8_t sample_rate) { this->sample_rate_ = sample_rate; }

 protected:
  ADS1100Component *parent_;
  uint8_t gain_{1};
  uint8_t sample_rate_{128};
};

}  // namespace ads1100
}  // namespace esphome