#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

#include "max7219digit.h"

namespace esphome {
namespace max7219digit {

template<typename... Ts> class DisplayInvertAction : public Action<Ts...>, public Parented<MAX7219Component> {
 public:
  TEMPLATABLE_VALUE(bool, state)

  void play(Ts... x) override { this->parent_->invert_on_off(this->state_.optional_value(x...)); }
};

template<typename... Ts> class DisplayVisiblityAction : public Action<Ts...>, public Parented<MAX7219Component> {
 public:
  TEMPLATABLE_VALUE(bool, state)

  void play(Ts... x) override { this->parent_->turn_on_off(this->state_.optional_value(x...)); }
};

template<typename... Ts> class DisplayReverseAction : public Action<Ts...>, public Parented<MAX7219Component> {
 public:
  TEMPLATABLE_VALUE(bool, state)

  void play(Ts... x) override { this->parent_->set_reverse(this->state_.optional_value(x...)); }
};

template<typename... Ts> class DisplayIntensityAction : public Action<Ts...>, public Parented<MAX7219Component> {
 public:
  TEMPLATABLE_VALUE(uint8_t, state)

  void play(Ts... x) override { this->parent_->intensity(this->state_.optional_value(x...)); }
};

}  // namespace max7219digit
}  // namespace esphome
