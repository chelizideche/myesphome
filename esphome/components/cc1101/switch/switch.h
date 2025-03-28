#pragma once

#include "esphome/components/switch/switch.h"
#include "../cc1101.h"

namespace esphome {
namespace cc1101 {

class DcBlockingFilterSwitch : public switch_::Switch, public Parented<CC1101Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_dc_blocking_filter(value);
  }
};

class AgcLnaPrioritySwitch : public switch_::Switch, public Parented<CC1101Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_agc_lna_priority(value);
  }
};

}  // namespace cc1101
}  // namespace esphome
