#include "decrease.h"

namespace esphome {
namespace vornado_ir {

static const char *const TAG = "vornado_ir.decrease_button";

void DecreaseButton::press_action() { this->parent_->send_decrease(); }

}  // namespace vornado_ir
}  // namespace esphome
