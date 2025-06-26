#include "gp8211_output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gp8211 {

static const char *const TAG = "gp8211.output";

static const uint8_t OUTPUT_REGISTER = 0x02;

void GP8211Output::dump_config() { ESP_LOGCONFIG(TAG, "GP8211 Output:"); }

void GP8211Output::write_state(float state) {
  uint16_t value = static_cast<uint16_t>(state * 32767);
  ESP_LOGV(TAG, "Calculated DAC value: %u", value);

  i2c::ErrorCode err = this->parent_->write_register(OUTPUT_REGISTER, (uint8_t *) &value, 2);

  if (err != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Error writing to GP8211, code %d", err);
    this->status_set_error();
    return;
  }
  this->status_clear_error();
}

}  // namespace gp8211
}  // namespace esphome
