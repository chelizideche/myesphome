#include "reset_button.h"

namespace esphome {
namespace tfluna {

void ResetButton::press_action() { this->parent_->factory_reset(); }

}  // namespace tfluna
}  // namespace esphome
