#pragma once

#include "esphome/components/button/button.h"
#include "../hpma115c0.h"

namespace esphome {
namespace hpma115c0 {

class Hpma115C0FactoryDefaultButton : public button::Button, public Parented<Hpma115C0PollingComponent> {
 public:
  Hpma115C0FactoryDefaultButton() = default;

 protected:
  void press_action() override;
};

}  // namespace hpma115c0
}  // namespace esphome
