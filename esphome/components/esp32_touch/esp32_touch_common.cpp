#ifdef USE_ESP32

#include "esp32_touch.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace esp32_touch {

static const char *const TAG = "esp32_touch";

void ESP32TouchComponent::dump_config_base_() {
  const char *lv_s = get_low_voltage_reference_str(this->low_voltage_reference_);
  const char *hv_s = get_high_voltage_reference_str(this->high_voltage_reference_);
  const char *atten_s = get_voltage_attenuation_str(this->voltage_attenuation_);

  ESP_LOGCONFIG(TAG,
                "Config for ESP32 Touch Hub:\n"
                "  Meas cycle: %.2fms\n"
                "  Sleep cycle: %.2fms\n"
                "  Low Voltage Reference: %s\n"
                "  High Voltage Reference: %s\n"
                "  Voltage Attenuation: %s\n"
                "  ISR Configuration:\n"
                "    Release timeout: %" PRIu32 "ms\n"
                "    Release check interval: %" PRIu32 "ms",
                this->meas_cycle_ / (8000000.0f / 1000.0f), this->sleep_cycle_ / (150000.0f / 1000.0f), lv_s, hv_s,
                atten_s, this->release_timeout_ms_, this->release_check_interval_ms_);
}

void ESP32TouchComponent::dump_config_sensors_() {
  for (auto *child : this->children_) {
    LOG_BINARY_SENSOR("  ", "Touch Pad", child);
    ESP_LOGCONFIG(TAG, "    Pad: T%" PRIu32, (uint32_t) child->get_touch_pad());
    ESP_LOGCONFIG(TAG, "    Threshold: %" PRIu32, child->get_threshold());
  }
}

}  // namespace esp32_touch
}  // namespace esphome

#endif  // USE_ESP32