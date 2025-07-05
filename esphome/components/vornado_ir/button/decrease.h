#pragma once

#include "esphome/components/button/button.h"
#include "../vornado_ir.h"

namespace esphome {
namespace vornado_ir {

class DecreaseButton : public button::Button, public Parented<VornadoIR> {
 public:
  DecreaseButton() = default;

 protected:
  void press_action() override;
};

}  // namespace vornado_ir
}  // namespace esphome
