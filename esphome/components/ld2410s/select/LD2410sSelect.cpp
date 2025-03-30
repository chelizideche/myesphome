#include "LD2410sSelect.h"

namespace esphome {
namespace ld2410s {

void LD2410sResponseSpeedSelect::control(const std::string &response_speed_select) {
  this->publish_state(response_speed_select);
  this->parent_->set_response_speed_select(response_speed_select);
}

}  // namespace ld2410s
}  // namespace esphome
