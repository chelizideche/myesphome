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
 *
 * All "number" configuration (min, max, step, mode) is handled via the
 * traits member (type NumberTraits) inherited from number::Number.
 */
class HamulightBrightnessNumber : public number::Number {
 public:
  /**
   * @brief Set the callback function to be called when the number is set.
   * @param cb Function taking the new float value.
   */
  void set_callback(std::function<void(float)> cb) { cb_ = std::move(cb); }

  /**
   * @brief Configure the number traits (min, max, step, mode).
   * Call this after construction, before registering the entity.
   */
  void setup_hamulight_traits() {
    this->traits.set_min_value(0);
    this->traits.set_max_value(100);
    this->traits.set_step(1);
    this->traits.set_mode(number::NumberMode::NUMBER_MODE_SLIDER);
  }

 protected:
  /**
   * @brief Called by ESPHome when the number is set from HA or automation.
   * Calls the callback if set, then publishes the new state.
   * @param value The new value.
   */
  void control(float value) override {
    if (cb_)
      cb_(value);
    publish_state(value); // Keep UI in sync with HA
  }

  std::function<void(float)> cb_; ///< Callback for value change
};

}  // namespace hamulight
}  // namespace esphome
