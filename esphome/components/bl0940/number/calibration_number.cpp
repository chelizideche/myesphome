#include "calibration_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bl0940 {

static const char *const TAG = "bl0940.number";

void CalibrationNumber::setup() {
  if (this->f_.has_value())
    return;

  float value;
  if (!this->restore_value_) {
    value = this->initial_value_;
  } else {
    this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
    if (!this->pref_.load(&value)) {
      if (!std::isnan(this->initial_value_)) {
        value = this->initial_value_;
      } else {
        value = this->traits.get_min_value();
      }
    }
  }
  this->publish_state(value);
}

void CalibrationNumber::update() {
  if (!this->f_.has_value())
    return;

  auto val = (*this->f_)();
  if (!val.has_value())
    return;

  this->publish_state(*val);
}

void CalibrationNumber::control(float value) {
  this->publish_state(value);
  if (this->restore_value_)
    this->pref_.save(&value);
}

void CalibrationNumber::dump_config() {
  LOG_NUMBER("", "Calibration Number", this);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace bl0940
}  // namespace esphome
