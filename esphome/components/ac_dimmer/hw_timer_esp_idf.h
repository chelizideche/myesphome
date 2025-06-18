#pragma once
#ifdef USE_ESP32_FRAMEWORK_ESP_IDF

#include "esp_idf_version.h"

#if (ESP_IDF_VERSION_MAJOR == 4)
#include "driver/timer.h"
#endif

#if (ESP_IDF_VERSION_MAJOR == 5)
#include "driver/gptimer_types.h"
#endif

namespace esphome {
namespace ac_dimmer {

#if (ESP_IDF_VERSION_MAJOR == 4)
typedef struct hw_timer_s {
  timer_group_t group;
  timer_idx_t num;
} hw_timer_t;

hw_timer_t *timer_begin(uint8_t timer, uint16_t divider, bool count_up);
void timer_attach_interrupt(hw_timer_t *timer, void (*fn)(void), bool edge);
void timer_alarm_enable(hw_timer_t *timer);
void timer_alarm_disable(hw_timer_t *timer);
void timer_alarm_write(hw_timer_t *timer, uint64_t alarm_value, bool autoreload);
void timer_set_auto_reload(hw_timer_t *timer, bool autoreload);
void timer_start(hw_timer_t *timer);
void timer_stop(hw_timer_t *timer);

uint32_t timer_get_config(hw_timer_t *timer);
uint16_t timer_get_divider(hw_timer_t *timer);
void timer_set_divider(hw_timer_t *timer, uint16_t divider);
#endif

#if (ESP_IDF_VERSION_MAJOR == 5)

using void_func_ptr = void (*)();
using void_func_ptr_arg = void (*)(void *);

// keep structure names as in SDK
struct interrupt_config_t {  // NOLINT
  void_func_ptr fn;
  void *arg;
};

// keep structure names as in SDK
struct hw_timer_t {  // NOLINT
  gptimer_handle_t timer_handle;
  interrupt_config_t interrupt_handle;
  bool timer_started;
};

hw_timer_t *timer_begin(uint32_t frequency);

void timer_attach_interrupt(hw_timer_t *timer, void (*userFunc)(void));
void timer_alarm(hw_timer_t *timer, uint64_t alarm_value, bool autoreload, uint64_t reload_count);
void timer_start(hw_timer_t *timer);

#endif
}  // namespace ac_dimmer
}  // namespace esphome

#endif
