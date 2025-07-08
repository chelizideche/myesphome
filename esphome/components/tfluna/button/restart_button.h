#pragma once

#include "esphome/components/button/button.h"
#include "../tfluna.h"

namespace esphome {
namespace tfluna {

class RestartButton : public button::Button, public Parented<TFLuna> {
 public:
  RestartButton() = default;

 protected:
  void press_action() override;
};

}  // namespace tfluna
}  // namespace esphome
