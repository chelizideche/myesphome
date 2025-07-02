#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "dallas_pio_constants.h"
#include "dallas_pio.h"

namespace esphome {
namespace dallas_pio {

class DallasPioSwitch : public Component, public switch_::Switch {
 public:
  DallasPioSwitch() : dallas_pio_(nullptr), pio_pin_(0) {}
  DallasPioSwitch(DallasPio *dallas_pio, uint8_t pio_pin) : dallas_pio_(dallas_pio), pio_pin_(pio_pin) {}
  void setup() override;
  void dump_config() override;
  void loop() override;
  void write_state(bool state) override;

  using EntityBase::set_name;
  void set_dallas_pio(DallasPio *dallas_pio) { this->dallas_pio_ = dallas_pio; }
  void set_address(uint64_t address) { this->address_ = address; }
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_pin_inverted(bool pin_inverted) { this->pin_inverted_ = pin_inverted; }
  void set_pin_mode(bool input, bool output) {
    this->is_input_ = input;
    this->is_output_ = output;
  }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }

 protected:
  DallasPio *dallas_pio_;
  uint8_t pio_pin_;
  std::string reference_;
  uint8_t family_code_;
  uint64_t address_;
  uint8_t pin_;
  bool pin_inverted_;
  bool inverted_;
  bool is_input_;
  bool is_output_;
};

}  // namespace dallas_pio
}  // namespace esphome
