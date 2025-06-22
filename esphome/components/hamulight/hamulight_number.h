#pragma once

#include "esphome/components/number/number.h"

namespace esphome {
namespace hamulight {

/**
 * @brief A custom ESPHome number (slider) entity for Hamulight brightness.
 *
 * This number calls a C++ callback when its value is set, and exposes
 * the entity to Home Assistant. Any YAML on_value automations
 * are also triggered by ESPHome after the control() call.
 */
class HamulightBrightnessNumber : public number::Number {
 public:
  void set_callback(std::function<void(float)> cb) { cb_ = std::move(cb); }
 protected:
  void control(float value) override {
    if (cb_)
      cb_(value);
    publish_state(value); // Keep UI in sync
  }
  std::function<void(float)> cb_;
};

}  // namespace hamulight
}  // namespace esphome
