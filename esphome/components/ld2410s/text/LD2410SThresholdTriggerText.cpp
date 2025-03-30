#include "LD2410SThresholdTriggerText.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ld2410s {

// static const char *const TAG = "copy.text";

// void LD2410SText::setup() {
//   source_->add_on_state_callback([this](const std::string &value) { this->publish_state(value); });

//   if (source_->has_state())
//     this->publish_state(source_->state);
// }

// void LD2410SText::dump_config() { LOG_TEXT("", "Copy Text", this); }

// void LD2410SText::control(const std::string &value) {
//   auto call2 = source_->make_call();
//   call2.set_value(value);
//   call2.perform();
// }

void LD2410SThresholdTriggerText::control(const std::string &value) {
  this->publish_state(value);
  this->parent_->new_config.thresholds_trigger = value;
}

}  // namespace ld2410s
}  // namespace esphome
