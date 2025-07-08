#pragma once

#include "esphome/core/helpers.h"

namespace esphome {
namespace switch_ {
class Switch;

class SwitchCall {
 public:
  explicit SwitchCall(Switch *parent) : parent_(parent) {}
  void perform();

  SwitchCall &set_state(bool state);
  SwitchCall &set_inverted(bool inverted);

 protected:
  Switch *const parent_;
  optional<bool> state_;
  optional<bool> inverted_;
};
}  // namespace switch_
}  // namespace esphome
