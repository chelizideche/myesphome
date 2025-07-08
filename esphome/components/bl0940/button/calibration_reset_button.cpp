#include "calibration_reset_button.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace bl0940 {

static const char *const TAG = "calibration_reset.button";

void CalibrationResetButton::dump_config() { LOG_BUTTON("", "Calibration Reset Button", this); }
void CalibrationResetButton::press_action() { ESP_LOGI(TAG, "Resetting calibration defaults..."); }

}  // namespace bl0940
}  // namespace esphome
