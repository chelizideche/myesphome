#include "ds2406_switch.h"

namespace esphome {
namespace ds2406 {

void Ds2406Switch::write_state(bool state) {
  if (this->channel_ != 0) {
    this->parent_->write_state(this->channel_, state);
    this->publish_state(state);
  }
}

}  // namespace ds2406
}  // namespace esphome
