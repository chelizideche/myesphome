#include "adjustment_number.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

static const char *const TAG = "hpma115c0.number";

namespace esphome {
namespace hpma115c0 {

void Hpma115C0AdjustmentNumber::control(float adjustment_coefficient) {
  AdjustmentCoefficient_t new_value = (AdjustmentCoefficient_t) adjustment_coefficient;

  if (new_value < ADJUSTMENT_COEFFICIENT_MIN) {
    new_value = ADJUSTMENT_COEFFICIENT_MIN;
  } else if (new_value > ADJUSTMENT_COEFFICIENT_MAX) {
    new_value = ADJUSTMENT_COEFFICIENT_MIN;
  }

  parent_->update_adjustment_coefficient(adjustment_coefficient);

  this->publish_state(new_value);
}

}  // namespace hpma115c0
}  // namespace esphome
