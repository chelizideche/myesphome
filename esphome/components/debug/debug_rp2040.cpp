#include "debug_component.h"
#ifdef USE_RP2040
#include "esphome/core/log.h"
#include <Arduino.h>
namespace esphome {
namespace debug {

static const char *const TAG = "debug";

std::string DebugComponent::get_reset_reason_() {
  switch (rp2040.getResetReason()) {
    case RP2040::PWRON_RESET:
      return "pwron_reset";
    case RP2040::RUN_PIN_RESET:
      return "run_pin_reset";
    case RP2040::SOFT_RESET:
      return "soft_reset";
    case RP2040::WDT_RESET:
      return "wdt_reset";
    case RP2040::DEBUG_RESET:
      return "debug_reset";
    case RP2040::GLITCH_RESET:
      return "glitch_reset";
    case RP2040::BROWNOUT_RESET:
      return "brownout_reset";
    case RP2040::UNKNOWN_RESET:
      return "unknown_reason";
  }
  return "";
}

uint32_t DebugComponent::get_free_heap_() { return rp2040.getFreeHeap(); }

void DebugComponent::get_device_info_(std::string &device_info) {
  ESP_LOGD(TAG, "CPU Frequency: %u", rp2040.f_cpu());
  device_info += "CPU Frequency: " + to_string(rp2040.f_cpu());
}

void DebugComponent::update_platform_() {}

}  // namespace debug
}  // namespace esphome
#endif
