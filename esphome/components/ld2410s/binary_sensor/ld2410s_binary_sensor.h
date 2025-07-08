#pragma once

#include "../ld2410s.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace ld2410s {
class LD2410SBinarySensor : public LD2410SListener, public Component, binary_sensor::BinarySensor {
 public:
  void set_presence_sensor(binary_sensor::BinarySensor *bsensor) { this->presence_bsensor_ = bsensor; };
  void set_threshold_update_sensor(binary_sensor::BinarySensor *bsensor) { this->threshold_update_bsensor_ = bsensor; };
  void on_presence(bool presence) override {
    if (this->presence_bsensor_ != nullptr) {
      if (this->presence_bsensor_->state != presence) {
        this->presence_bsensor_->publish_state(presence);
      }
    }
  };
  void on_threshold_update(bool running) override {
    if (this->threshold_update_bsensor_ != nullptr) {
      if (this->threshold_update_bsensor_->state != running) {
        this->threshold_update_bsensor_->publish_state(running);
      }
    }
  };

 private:
  binary_sensor::BinarySensor *presence_bsensor_{nullptr};
  binary_sensor::BinarySensor *threshold_update_bsensor_{nullptr};
};
}  // namespace ld2410s
}  // namespace esphome
