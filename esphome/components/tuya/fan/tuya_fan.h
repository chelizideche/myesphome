#pragma once

#include <set>

#include "esphome/core/component.h"
#include "esphome/components/tuya/tuya.h"
#include "esphome/components/fan/fan.h"

namespace esphome {
namespace tuya {

class TuyaFan : public Component, public fan::Fan {
 public:
  TuyaFan(Tuya *parent, int speed_count) : parent_(parent), speed_count_(speed_count) {}
  void setup() override;
  void dump_config() override;
  void set_speed_id(uint8_t speed_id) { this->speed_id_ = speed_id; }
  void set_switch_id(uint8_t switch_id) { this->switch_id_ = switch_id; }
  void set_oscillation_id(uint8_t oscillation_id) { this->oscillation_id_ = oscillation_id; }
  void set_direction_id(uint8_t direction_id) { this->direction_id_ = direction_id; }
  void set_preset_modes(const std::set<std::string> &presets) { this->preset_modes_ = presets; }
  fan::FanTraits get_traits() override { return this->traits_; }

 protected:
  void control(const fan::FanCall &call) override;

  Tuya *parent_;
  optional<uint8_t> speed_id_{};
  optional<uint8_t> switch_id_{};
  optional<uint8_t> oscillation_id_{};
  optional<uint8_t> direction_id_{};
  int speed_count_{};
  TuyaDatapointType speed_type_{};
  TuyaDatapointType oscillation_type_{};
  fan::FanTraits traits_;
  std::set<std::string> preset_modes_{};
};

}  // namespace tuya
}  // namespace esphome
