#pragma once

#include "esphome/core/component.h"
#include "esphome/components/atm90e32/atm90e32.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace atm90e32 {

class ATM90E32PhaseStatusSensor : public text_sensor::TextSensor, public Parented<ATM90E32Component> {
 public:
  void set_parent(ATM90E32Component *parent) { this->parent_ = parent; }
};

class ATM90E32FreqStatusSensor : public text_sensor::TextSensor, public Parented<ATM90E32Component> {
 public:
  void set_parent(ATM90E32Component *parent) { this->parent_ = parent; }
};

}  // namespace atm90e32
}  // namespace esphome