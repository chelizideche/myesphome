#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace bl0940 {

class CalibrationNumber : public number::Number, public PollingComponent {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_restore_value(bool restore_value) { this->restore_value_ = restore_value; }

 protected:
  void control(float value) override;
  float initial_value_{0};
  bool restore_value_{true};
  optional<std::function<optional<float>()>> f_;

  ESPPreferenceObject pref_;
};

}  // namespace bl0940
}  // namespace esphome
