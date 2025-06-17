#pragma once

#include "esphome/components/switch/switch.h"

#include "../ds2406.h"

namespace esphome {
namespace ds2406 {

class Ds2406Switch : public switch_::Switch, public Parented<Ds2406> {
 public:
  void write_state(bool state) override;
  void set_channel(uint8_t channel) { this->channel_ = channel; }

 protected:
  uint8_t channel_{0};
};

}  // namespace ds2406
}  // namespace esphome
