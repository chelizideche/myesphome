#include "hpma115c0_button.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

static const char *const TAG = "hpma115c0.button";

namespace esphome {
namespace hpma115c0 {

void Hpma115C0FactoryDefaultButton::press_action() { this->parent_->factory_reset_action(); }

}  // namespace hpma115c0
}  // namespace esphome
