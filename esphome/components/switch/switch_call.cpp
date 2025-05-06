#include "switch_call.h"
#include "switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace switch_ {

static const char *const TAG = "switch";

SwitchCall &SwitchCall::set_state(bool state) {
  this->state_ = state;
  return *this;
}
SwitchCall &SwitchCall::set_inverted(bool inverted) {
  this->inverted_ = inverted;
  return *this;
}

void SwitchCall::perform() {
  auto *parent = this->parent_;
  const auto *name = parent->get_name().c_str();

  if (this->inverted_.has_value()) {
    ESP_LOGD(TAG, "'%s' - Setting inversion: %s", name, ONOFF(this->inverted_.value()));
    parent->set_inverted(this->inverted_.value());
  }

  if (this->state_.has_value()) {
    ESP_LOGD(TAG, "'%s' - Setting state to %s", name, ONOFF(this->state_.value()));
  }
}
}  // namespace switch_
}  // namespace esphome