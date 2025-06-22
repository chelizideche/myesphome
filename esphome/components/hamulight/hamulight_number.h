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
 * The class implements its own setters/getters for min/max/step/mode,
 * so it can be configured just like a stock ESPHome number.
 */
class HamulightBrightnessNumber : public number::Number {
 public:
  HamulightBrightnessNumber()
      : min_value_(0), max_value_(100), step_(1), mode_(number::NumberMode::NUMBER_MODE_SLIDER) {}

  /**
   * @brief Set the callback function to be called when the number is set.
   * @param cb Function taking the new float value.
   */
  void set_callback(std::function<void(float)> cb) { cb_ = std::move(cb); }

  void set_min_value(float val) { min_value_ = val; }
  void set_max_value(float val) { max_value_ = val; }
  void set_step(float val) { step_ = val; }
  void set_mode(number::NumberMode mode) { mode_ = mode; }

  float get_min_value() const override { return min_value_; }
  float get_max_value() const override { return max_value_; }
  float get_step() const override { return step_; }
  number::NumberMode get_mode() const override { return mode_; }

 protected:
  /**
   * @brief Called by ESPHome when the number is set from HA or automation.
   * Calls the callback if set, then publishes the new state.
   * @param value The new value.
   */
  void control(float value) override {
    // Clamp value to min/max and adjust to step
    float v = value;
    if (v < min_value_) v = min_value_;
    if (v > max_value_) v = max_value_;
    // Optionally round to step here if desired

    if (cb_) cb_(v);
    publish_state(v); // Keep UI in sync with HA
  }

  std::function<void(float)> cb_; ///< Callback for value change

  float min_value_;
  float max_value_;
  float step_;
  number::NumberMode mode_;
};

}  // namespace hamulight
}  // namespace esphome
