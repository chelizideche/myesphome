#pragma once

#include "esphome/components/number/number.h"
#include "../hpma115c0.h"

namespace esphome {
namespace hpma115c0 {

class Hpma115C0AdjustmentNumber : public number::Number, public Parented<Hpma115C0PollingComponent> {
 public:
  Hpma115C0AdjustmentNumber() = default;

 protected:
  void control(float timeout) override;
};

}  // namespace hpma115c0
}  // namespace esphome
