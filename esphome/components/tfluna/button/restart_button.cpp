#include "restart_button.h"

namespace esphome {
namespace tfluna {

void RestartButton::press_action() { this->parent_->restart(); }

}  // namespace tfluna
}  // namespace esphome
