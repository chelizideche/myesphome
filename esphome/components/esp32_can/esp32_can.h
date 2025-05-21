#pragma once
#include "esphome/core/defines.h"
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
  void set_rx(int rx) { rx_ = rx; }
  void set_tx(int tx) { tx_ = tx; }
  void set_tx_queue_len(uint32_t tx_queue_len) { this->tx_queue_len_ = tx_queue_len; }
  void set_rx_queue_len(uint32_t rx_queue_len) { this->rx_queue_len_ = rx_queue_len; }
  void set_tx_enqueue_timeout_ms(uint32_t tx_enqueue_timeout_ms) {
    this->tx_enqueue_timeout_ticks_ = pdMS_TO_TICKS(tx_enqueue_timeout_ms);
  }
  ESP32Can() {}

  void loop() override;

 protected:
  bool setup_internal() override;
  canbus::Error send_message(struct canbus::CanFrame *frame) override;
  canbus::Error read_message(struct canbus::CanFrame *frame) override;

  int rx_{-1};
  int tx_{-1};
  TickType_t tx_enqueue_timeout_ticks_{};
  optional<uint32_t> tx_queue_len_{};
  optional<uint32_t> rx_queue_len_{};

#ifdef ESP32_CAN_V2_SUPPORTED
  twai_handle_t twai_handle_{};
#endif
  twai_state_t twai_last_state_ = static_cast<twai_state_t>(-1);
  optional<twai_status_info_t> prev_status_;
};

}  // namespace esp32_can
}  // namespace esphome

#endif
