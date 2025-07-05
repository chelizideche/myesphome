#pragma once

#include "esphome/components/button/button.h"
#include "../vornado_ir.h"

namespace esphome {
namespace vornado_ir {

class ChangeDirectionButton : public button::Button, public Parented<VornadoIR> {
 public:
  ChangeDirectionButton() = default;

 protected:
  void press_action() override;
};

}  // namespace vornado_ir
}  // namespace esphome
