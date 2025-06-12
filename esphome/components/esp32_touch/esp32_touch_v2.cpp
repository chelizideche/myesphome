#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)

#include "esp32_touch.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#include <algorithm>
#include <cinttypes>

// Include HAL for ISR-safe touch reading
#include "hal/touch_sensor_ll.h"
// Include for RTC clock frequency
#include "soc/rtc.h"
// Include for ISR-safe printing
#include "rom/ets_sys.h"

namespace esphome {
namespace esp32_touch {

static const char *const TAG = "esp32_touch";

void ESP32TouchComponent::setup() {
  ESP_LOGCONFIG(TAG, "Running setup for ESP32-S2/S3");

  touch_pad_init();
  touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);

  // Create queue for touch events
  size_t queue_size = this->children_.size() * 4;
  if (queue_size < 8)
    queue_size = 8;

  this->touch_queue_ = xQueueCreate(queue_size, sizeof(TouchPadEvent));
  if (this->touch_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create touch event queue of size %d", queue_size);
    this->mark_failed();
    return;
  }

  // Set up filtering if configured
  if (this->filter_configured_()) {
    touch_filter_config_t filter_info = {
        .mode = this->filter_mode_,
        .debounce_cnt = this->debounce_count_,
        .noise_thr = this->noise_threshold_,
        .jitter_step = this->jitter_step_,
        .smh_lvl = this->smooth_level_,
    };
    touch_pad_filter_set_config(&filter_info);
    touch_pad_filter_enable();
  }

  if (this->denoise_configured_()) {
    touch_pad_denoise_t denoise = {
        .grade = this->grade_,
        .cap_level = this->cap_level_,
    };
    touch_pad_denoise_set_config(&denoise);
    touch_pad_denoise_enable();
  }

  if (this->waterproof_configured_()) {
    touch_pad_waterproof_t waterproof = {
        .guard_ring_pad = this->waterproof_guard_ring_pad_,
        .shield_driver = this->waterproof_shield_driver_,
    };
    touch_pad_waterproof_set_config(&waterproof);
    touch_pad_waterproof_enable();
  }

  // Configure measurement parameters
  touch_pad_set_charge_discharge_times(this->meas_cycle_);
  touch_pad_set_measurement_interval(this->sleep_cycle_);
  touch_pad_set_voltage(this->high_voltage_reference_, this->low_voltage_reference_, this->voltage_attenuation_);

  // Set up the channel mask for all configured pads
  uint16_t channel_mask = 0;
  for (auto *child : this->children_) {
    channel_mask |= BIT(child->get_touch_pad());
  }
  touch_pad_set_channel_mask(channel_mask);

  // Configure each touch pad
  for (auto *child : this->children_) {
    // Initialize the touch pad
    touch_pad_config(child->get_touch_pad());

    // Set threshold
    if (child->get_threshold() != 0) {
      touch_pad_set_thresh(child->get_touch_pad(), child->get_threshold());
    }
  }

  // Configure timeout
  touch_pad_timeout_set(true, TOUCH_PAD_THRESHOLD_MAX);

  // Register ISR handler with all interrupts
  esp_err_t err =
      touch_pad_isr_register(touch_isr_handler, this, static_cast<touch_pad_intr_mask_t>(TOUCH_PAD_INTR_MASK_ALL));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register touch ISR: %s", esp_err_to_name(err));
    vQueueDelete(this->touch_queue_);
    this->touch_queue_ = nullptr;
    this->mark_failed();
    return;
  }

  // Calculate release timeout based on sleep cycle
  uint32_t rtc_freq = rtc_clk_slow_freq_get_hz();
  this->release_timeout_ms_ = (this->sleep_cycle_ * 1000 * 3) / (rtc_freq * 2);
  if (this->release_timeout_ms_ < 100) {
    this->release_timeout_ms_ = 100;
  }
  this->release_check_interval_ms_ = std::min(this->release_timeout_ms_ / 4, (uint32_t) 50);

  // Enable the interrupts we need
  touch_pad_intr_enable(static_cast<touch_pad_intr_mask_t>(TOUCH_PAD_INTR_MASK_ACTIVE | TOUCH_PAD_INTR_MASK_INACTIVE |
                                                           TOUCH_PAD_INTR_MASK_TIMEOUT));

  // Start the FSM after all configuration is complete
  touch_pad_fsm_start();
}

void ESP32TouchComponent::dump_config() {
  this->dump_config_base_();

  if (this->filter_configured_()) {
    const char *filter_mode_s;
    switch (this->filter_mode_) {
      case TOUCH_PAD_FILTER_IIR_4:
        filter_mode_s = "IIR_4";
        break;
      case TOUCH_PAD_FILTER_IIR_8:
        filter_mode_s = "IIR_8";
        break;
      case TOUCH_PAD_FILTER_IIR_16:
        filter_mode_s = "IIR_16";
        break;
      case TOUCH_PAD_FILTER_IIR_32:
        filter_mode_s = "IIR_32";
        break;
      case TOUCH_PAD_FILTER_IIR_64:
        filter_mode_s = "IIR_64";
        break;
      case TOUCH_PAD_FILTER_IIR_128:
        filter_mode_s = "IIR_128";
        break;
      case TOUCH_PAD_FILTER_IIR_256:
        filter_mode_s = "IIR_256";
        break;
      case TOUCH_PAD_FILTER_JITTER:
        filter_mode_s = "JITTER";
        break;
      default:
        filter_mode_s = "UNKNOWN";
        break;
    }
    ESP_LOGCONFIG(TAG,
                  "  Filter mode: %s\n"
                  "  Debounce count: %" PRIu32 "\n"
                  "  Noise threshold coefficient: %" PRIu32 "\n"
                  "  Jitter filter step size: %" PRIu32,
                  filter_mode_s, this->debounce_count_, this->noise_threshold_, this->jitter_step_);
    const char *smooth_level_s;
    switch (this->smooth_level_) {
      case TOUCH_PAD_SMOOTH_OFF:
        smooth_level_s = "OFF";
        break;
      case TOUCH_PAD_SMOOTH_IIR_2:
        smooth_level_s = "IIR_2";
        break;
      case TOUCH_PAD_SMOOTH_IIR_4:
        smooth_level_s = "IIR_4";
        break;
      case TOUCH_PAD_SMOOTH_IIR_8:
        smooth_level_s = "IIR_8";
        break;
      default:
        smooth_level_s = "UNKNOWN";
        break;
    }
    ESP_LOGCONFIG(TAG, "  Smooth level: %s", smooth_level_s);
  }

  if (this->denoise_configured_()) {
    const char *grade_s;
    switch (this->grade_) {
      case TOUCH_PAD_DENOISE_BIT12:
        grade_s = "BIT12";
        break;
      case TOUCH_PAD_DENOISE_BIT10:
        grade_s = "BIT10";
        break;
      case TOUCH_PAD_DENOISE_BIT8:
        grade_s = "BIT8";
        break;
      case TOUCH_PAD_DENOISE_BIT4:
        grade_s = "BIT4";
        break;
      default:
        grade_s = "UNKNOWN";
        break;
    }
    ESP_LOGCONFIG(TAG, "  Denoise grade: %s", grade_s);

    const char *cap_level_s;
    switch (this->cap_level_) {
      case TOUCH_PAD_DENOISE_CAP_L0:
        cap_level_s = "L0";
        break;
      case TOUCH_PAD_DENOISE_CAP_L1:
        cap_level_s = "L1";
        break;
      case TOUCH_PAD_DENOISE_CAP_L2:
        cap_level_s = "L2";
        break;
      case TOUCH_PAD_DENOISE_CAP_L3:
        cap_level_s = "L3";
        break;
      case TOUCH_PAD_DENOISE_CAP_L4:
        cap_level_s = "L4";
        break;
      case TOUCH_PAD_DENOISE_CAP_L5:
        cap_level_s = "L5";
        break;
      case TOUCH_PAD_DENOISE_CAP_L6:
        cap_level_s = "L6";
        break;
      case TOUCH_PAD_DENOISE_CAP_L7:
        cap_level_s = "L7";
        break;
      default:
        cap_level_s = "UNKNOWN";
        break;
    }
    ESP_LOGCONFIG(TAG, "  Denoise capacitance level: %s", cap_level_s);
  }

  if (this->setup_mode_) {
    ESP_LOGCONFIG(TAG, "  Setup Mode ENABLED");
  }

  this->dump_config_sensors_();
}

void ESP32TouchComponent::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  bool should_print = this->setup_mode_ && now - this->setup_mode_last_log_print_ > 250;

  // Print debug info for all pads in setup mode
  if (should_print) {
    for (auto *child : this->children_) {
      uint32_t value = 0;
      touch_pad_read_raw_data(child->get_touch_pad(), &value);
      child->value_ = value;
      ESP_LOGD(TAG, "Touch Pad '%s' (T%" PRIu32 "): %" PRIu32, child->get_name().c_str(),
               (uint32_t) child->get_touch_pad(), value);
    }
    this->setup_mode_last_log_print_ = now;
  }

  // Process any queued touch events from interrupts
  TouchPadEvent event;
  while (xQueueReceive(this->touch_queue_, &event, 0) == pdTRUE) {
    // Handle timeout events
    if (event.intr_mask & TOUCH_PAD_INTR_MASK_TIMEOUT) {
      // Resume measurement after timeout
      touch_pad_timeout_resume();
      continue;
    }

    // Handle active/inactive events
    if (event.intr_mask & (TOUCH_PAD_INTR_MASK_ACTIVE | TOUCH_PAD_INTR_MASK_INACTIVE)) {
      // Process touch status for each pad
      for (auto *child : this->children_) {
        touch_pad_t pad = child->get_touch_pad();

        // Check if this pad is in the status mask
        if (event.pad_status & BIT(pad)) {
          // Read current value
          uint32_t value = 0;
          if (this->filter_configured_()) {
            touch_pad_filter_read_smooth(pad, &value);
          } else {
            touch_pad_read_raw_data(pad, &value);
          }

          child->value_ = value;

          // For S2/S3, higher value means touched
          bool is_touched = (event.intr_mask & TOUCH_PAD_INTR_MASK_ACTIVE) != 0;

          if (is_touched != child->last_state_) {
            child->last_state_ = is_touched;
            child->publish_state(is_touched);
            ESP_LOGV(TAG, "Touch Pad '%s' state: %s (value: %" PRIu32 ", threshold: %" PRIu32 ")",
                     child->get_name().c_str(), is_touched ? "ON" : "OFF", value, child->get_threshold());
          }
        }
      }
    }
  }
}

void ESP32TouchComponent::on_shutdown() {
  // Disable interrupts
  touch_pad_intr_disable(static_cast<touch_pad_intr_mask_t>(TOUCH_PAD_INTR_MASK_ACTIVE | TOUCH_PAD_INTR_MASK_INACTIVE |
                                                            TOUCH_PAD_INTR_MASK_TIMEOUT));
  touch_pad_isr_deregister(touch_isr_handler, this);
  if (this->touch_queue_) {
    vQueueDelete(this->touch_queue_);
  }

  // Check if any pad is configured for wakeup
  bool is_wakeup_source = false;
  for (auto *child : this->children_) {
    if (child->get_wakeup_threshold() != 0) {
      if (!is_wakeup_source) {
        is_wakeup_source = true;
        // Touch sensor FSM mode must be 'TOUCH_FSM_MODE_TIMER' to use it to wake-up.
        touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
      }
    }
  }

  if (!is_wakeup_source) {
    touch_pad_deinit();
  }
}

void IRAM_ATTR ESP32TouchComponent::touch_isr_handler(void *arg) {
  ESP32TouchComponent *component = static_cast<ESP32TouchComponent *>(arg);
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  // Read interrupt status and pad status
  TouchPadEvent event;
  event.intr_mask = touch_pad_read_intr_status_mask();
  event.pad_status = touch_pad_get_status();
  event.pad = touch_pad_get_current_meas_channel();

  // Send event to queue for processing in main loop
  xQueueSendFromISR(component->touch_queue_, &event, &xHigherPriorityTaskWoken);

  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }
}

}  // namespace esp32_touch
}  // namespace esphome

#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3