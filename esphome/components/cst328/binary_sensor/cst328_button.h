#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "../touchscreen/cst328_touchscreen.h"

namespace esphome {
namespace cst328 {

class CST328Button : public binary_sensor::BinarySensor,
                     public Component,
                     public CST328ButtonListener,
                     public Parented<CST328Touchscreen> {
 public:
  void setup() override {
    this->parent_->register_button_listener(this);
    this->publish_initial_state(false);
  }

  void dump_config() override { LOG_BINARY_SENSOR("", "CST328 Button", this); }

  void update_button(bool state) override { this->publish_state(state); }
};

}  // namespace cst328
}  // namespace esphome
