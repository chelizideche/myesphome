#include "factory_reset_button.h"
#ifdef USE_OPENTHREAD
#include "esphome/components/openthread/openthread.h"
#endif
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace factory_reset {

static const char *const TAG = "factory_reset.button";

void FactoryResetButton::dump_config() { LOG_BUTTON("", "Factory Reset Button", this); }
void FactoryResetButton::press_action() {
  ESP_LOGI(TAG, "Resetting");
  // Let MQTT settle a bit
  delay(100);  // NOLINT
#ifdef USE_OPENTHREAD
  openthread::global_openthread_component->on_factory_reset();
#else
  global_preferences->reset();
  App.safe_reboot();
#endif
}

#ifdef USE_OPENTHREAD
void FactoryResetSwitch::loop() {
  if (openthread::global_openthread_component->factory_reset_ready) {
    global_preferences->reset();
    App.safe_reboot();
  }
}
#endif

}  // namespace factory_reset
}  // namespace esphome
