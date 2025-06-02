#pragma once
#include "esp_err.h"
#include "esphome/core/defines.h"
#include "esphome/core/optional.h"
#include "hal/twai_types.h"
#include "soc/gpio_num.h"
#ifdef USE_ESP32

#ifdef ESP32_CAN_V2_FIND_OUT
#include "esp_idf_version.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
#define ESP32_CAN_V2_SUPPORTED
#endif
#endif  // ESP32_CAN_V2_FIND_OUT

#include "esphome/components/canbus/canbus.h"
#include "esphome/core/component.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include <driver/twai.h>

namespace esphome {
namespace esp32_can {

enum class TXMode {
  NORMAL = TWAI_MODE_NORMAL,
  NO_ACK = TWAI_MODE_NO_ACK,
  LISTEN_ONLY = TWAI_MODE_LISTEN_ONLY,
};

// using TXMode = twai_mode_t;

class ESP32Can : public canbus::Canbus {
#ifdef USE_SENSOR
  SUB_SENSOR(msgs_to_tx)
  SUB_SENSOR(msgs_to_rx)
  SUB_SENSOR(tx_error_counter)
  SUB_SENSOR(rx_error_counter)
  SUB_SENSOR(tx_failed_count)
  SUB_SENSOR(rx_missed_count)
  SUB_SENSOR(rx_overrun_count)
  SUB_SENSOR(arb_lost_count)
  SUB_SENSOR(bus_error_count)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(state)
#endif
 public:
  void set_rx(int rx) { this->g_config_.rx_io = static_cast<gpio_num_t>(rx); }
  void set_tx(int tx) { this->g_config_.tx_io = static_cast<gpio_num_t>(tx); }
  void set_tx_queue_len(uint32_t tx_queue_len) { this->g_config_.tx_queue_len = tx_queue_len; }
  void set_rx_queue_len(uint32_t rx_queue_len) { this->g_config_.rx_queue_len = rx_queue_len; }
  void set_tx_enqueue_timeout_ms(uint32_t tx_enqueue_timeout_ms) {
    this->tx_enqueue_timeout_ticks_ = pdMS_TO_TICKS(tx_enqueue_timeout_ms);
  }
  void set_tx_mode(TXMode mode) { this->g_config_.mode = static_cast<twai_mode_t>(mode); }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
  void sleep_allow_pd(bool arg) { this->g_config_.general_flags.sleep_allow_pd = arg ? 1 : 0; }
#endif
  ESP32Can() {}

  void loop() override;
  float get_setup_priority() const override { return 500 /* setup_priority::HARDWARE_LATE */; }

  void add_config_callback(
      std::function<void(const twai_general_config_t &g_config, const twai_timing_config_t &t_config,
                         const twai_filter_config_t &f_config, esp_err_t err)>
          foo) {
    this->config_callback_manager_.add(std::move(foo));
  }

 protected:
  bool setup_internal() override;
  canbus::Error send_message(struct canbus::CanFrame *frame) override;
  canbus::Error read_message(struct canbus::CanFrame *frame) override;

  bool install_and_start_();

  TickType_t tx_enqueue_timeout_ticks_{};

#ifdef ESP32_CAN_V2_SUPPORTED
  twai_handle_t twai_handle_{};
#endif
  twai_state_t twai_last_state_ = static_cast<twai_state_t>(-1);
  optional<twai_status_info_t> prev_status_;
  twai_general_config_t g_config_ = TWAI_GENERAL_CONFIG_DEFAULT(TWAI_IO_UNUSED, TWAI_IO_UNUSED, TWAI_MODE_NORMAL);
  twai_filter_config_t f_config_ = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  twai_timing_config_t t_config_;
  optional<twai_general_config_t> initialized_g_config_;
  CallbackManager<void(const twai_general_config_t &g_config, const twai_timing_config_t &t_config,
                       const twai_filter_config_t &f_config, esp_err_t err)>
      config_callback_manager_{};
};

}  // namespace esp32_can
}  // namespace esphome

#endif
