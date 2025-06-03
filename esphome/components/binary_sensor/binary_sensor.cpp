#include "binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {

namespace binary_sensor {

static const char *const TAG = "binary_sensor";

void BinarySensor::publish_state(bool state) {
  if (this->filter_list_ == nullptr) {
    this->send_state_internal(state);
  } else {
    this->filter_list_->input(state);
  }
}
void BinarySensor::publish_initial_state(bool state) {
  this->invalidate_state();
  this->publish_state(state);
}
void BinarySensor::send_state_internal(optional<bool> state) {
  if (!state.has_value())
    return;
  if (!this->has_state()) {
    ESP_LOGD(TAG, "'%s': Sending initial state %s", this->get_name().c_str(), ONOFF(state));
  } else {
    ESP_LOGD(TAG, "'%s': Sending state %s", this->get_name().c_str(), ONOFF(state));
  }
  this->set_state(state);
  // copy current state to the visible property for backwards compatibility
  this->state = state;
}

void BinarySensor::add_filter(Filter *filter) {
  filter->parent_ = this;
  if (this->filter_list_ == nullptr) {
    this->filter_list_ = filter;
  } else {
    Filter *last_filter = this->filter_list_;
    while (last_filter->next_ != nullptr)
      last_filter = last_filter->next_;
    last_filter->next_ = filter;
  }
}
void BinarySensor::add_filters(const std::vector<Filter *> &filters) {
  for (Filter *filter : filters) {
    this->add_filter(filter);
  }
}
bool BinarySensor::is_status_binary_sensor() const { return false; }

}  // namespace binary_sensor

}  // namespace esphome
