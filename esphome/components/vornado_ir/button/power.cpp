#include "power.h"

namespace esphome {
namespace vornado_ir {

static const char *const TAG = "vornado_ir.power_toggle_button";

void PowerButton::press_action() { this->parent_->send_power_toggle(); }

}  // namespace vornado_ir
}  // namespace esphome
