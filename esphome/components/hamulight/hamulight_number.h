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
 * The class forwards all base class setter methods for min/max/step/mode,
 * so it can be configured just like a stock ESPHome number.
 */
class HamulightBrightnessNumber : public number::Number {
 public:
  /**
   * @brief Set the callback function to be called when the number is set.
   * @param cb Function taking the new float value.
   */
  void set_callback(std::function<void(float)> cb) { cb_ = std::move(cb); }

  // Forward base class setter methods so this class can be configured like a stock Number.
  using number::Number::set_min_value;
  using number::Number::set_max_value;
  using number::Number::set_step;
  using number::Number::set_mode;

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
