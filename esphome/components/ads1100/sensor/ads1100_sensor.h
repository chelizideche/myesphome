#pragma once

#include "../ads1100.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace ads1100 {

class ADS1100Sensor : public sensor::Sensor, public PollingComponent {
 public:
  void set_parent(ADS1100Component *parent) { parent_ = parent; }
  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  ADS1100Component *parent_;
};

}  // namespace ads1100
}  // namespace esphome