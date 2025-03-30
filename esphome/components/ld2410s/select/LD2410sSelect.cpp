#include "LD2410sSelect.h"

void esphome::ld2410s::LD2410sResponseSpeedSelect::control(const std::string &response_speed_select) {
  this->publish_state(response_speed_select);
  this->parent_->set_response_speed_select(response_speed_select);
}

// void esphome::ld2410s::LD2410sExecCommandSelect::control(const std::string &value)
// {
//     this->publish_state(value);
//     this->parent_->new_config.exec_command = value == RESPONSE_SPEED_NORMAL ? 5 : 10;
// }