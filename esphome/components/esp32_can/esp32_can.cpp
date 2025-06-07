#include <cstring>
#include "driver/twai.h"
#include "esphome/core/defines.h"

#ifdef USE_ESP32
#include "esp32_can.h"
#include "esphome/core/log.h"

// WORKAROUND, because CAN_IO_UNUSED is just defined as (-1) in this version
// of the framework which does not work with -fpermissive
#undef CAN_IO_UNUSED
#define CAN_IO_UNUSED ((gpio_num_t) - 1)

#ifdef ESP32_CAN_V2_SUPPORTED
#define twai_driver_install(args...) twai_driver_install_v2(args, &this->twai_handle_)
#define twai_driver_uninstall() twai_driver_uninstall_v2(this->twai_handle_)
#define twai_start() twai_start_v2(this->twai_handle_)
#define twai_stop() twai_stop_v2(this->twai_handle_)
#define twai_initiate_recovery() twai_initiate_recovery_v2(this->twai_handle_)
#define twai_transmit(args...) twai_transmit_v2(this->twai_handle_, args)
#define twai_receive(args...) twai_receive_v2(this->twai_handle_, args)
#define twai_get_status_info(args...) twai_get_status_info_v2(this->twai_handle_, args)
#endif

namespace esphome {
namespace esp32_can {

static const char *const TAG = "esp32_can";

static bool get_bitrate(canbus::CanSpeed bitrate, twai_timing_config_t *t_config) {
  switch (bitrate) {
#ifdef TWAI_TIMING_CONFIG_1KBITS
    case canbus::CAN_1KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_1KBITS();
      return true;
#endif
#ifdef TWAI_TIMING_CONFIG_5KBITS
    case canbus::CAN_5KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_5KBITS();
      return true;
#endif

#ifdef TWAI_TIMING_CONFIG_10KBITS
    case canbus::CAN_10KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_10KBITS();
      return true;
#endif
#ifdef TWAI_TIMING_CONFIG_12_5KBITS
    case canbus::CAN_12K5BPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_12_5KBITS();
      return true;
#endif
#ifdef TWAI_TIMING_CONFIG_16KBITS
    case canbus::CAN_16KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_16KBITS();
      return true;
#endif
#ifdef TWAI_TIMING_CONFIG_20KBITS
    case canbus::CAN_20KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_20KBITS();
      return true;
#endif

#ifdef TWAI_TIMING_CONFIG_25KBITS
    case canbus::CAN_25KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_25KBITS();
      return true;
#endif
#ifdef TWAI_TIMING_CONFIG_50KBITS
    case canbus::CAN_50KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_50KBITS();
      return true;
#endif
#ifdef TWAI_TIMING_CONFIG_100KBITS
    case canbus::CAN_100KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_100KBITS();
      return true;
#endif
#ifdef TWAI_TIMING_CONFIG_125KBITS
    case canbus::CAN_125KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_125KBITS();
      return true;
#endif
#ifdef TWAI_TIMING_CONFIG_250KBITS
    case canbus::CAN_250KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_250KBITS();
      return true;
#endif
#ifdef TWAI_TIMING_CONFIG_500KBITS
    case canbus::CAN_500KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_500KBITS();
      return true;
#endif
#ifdef TWAI_TIMING_CONFIG_800KBITS
    case canbus::CAN_800KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_800KBITS();
      return true;
#endif
#ifdef TWAI_TIMING_CONFIG_1MBITS
    case canbus::CAN_1000KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_1MBITS();
      return true;
#endif
    default:
      return false;
  }
}

bool ESP32Can::setup_internal() {
#ifdef ESP32_CAN_V2_SUPPORTED
  static int controller_id = 0;
  this->g_config_.controller_id = controller_id++;
#endif

  ESP_LOGV(TAG, "Finding bitrate");
  if (!get_bitrate(this->bit_rate_, &this->t_config_)) {
    ESP_LOGE(TAG, "invalid bit rate");
    this->mark_failed();
    return false;
  }

  return this->install_and_start_();
}

canbus::Error ESP32Can::send_message(struct canbus::CanFrame *frame) {
  if (frame->can_data_length_code > canbus::CAN_MAX_DATA_LENGTH) {
    return canbus::ERROR_FAILTX;
  }

  uint32_t flags = TWAI_MSG_FLAG_NONE;
  if (frame->use_extended_id) {
    flags |= TWAI_MSG_FLAG_EXTD;
  }
  if (frame->remote_transmission_request) {
    flags |= TWAI_MSG_FLAG_RTR;
  }

  twai_message_t message = {
      .flags = flags,
      .identifier = frame->can_id,
      .data_length_code = frame->can_data_length_code,
      .data = {},  // to suppress warning, data is initialized properly below
  };
  if (!frame->remote_transmission_request) {
    memcpy(message.data, frame->data, frame->can_data_length_code);
  }

  const auto err = twai_transmit(&message, this->tx_enqueue_timeout_ticks_);
  if (err == ESP_OK) {
    return canbus::ERROR_OK;
  } else {
    ESP_LOGE(TAG, "Failed to send: %s", esp_err_to_name(err));
    return canbus::ERROR_ALLTXBUSY;
  }
}

canbus::Error ESP32Can::read_message(struct canbus::CanFrame *frame) {
  twai_message_t message;

  const auto err = twai_receive(&message, 0);
  switch (err) {
    case ESP_ERR_TIMEOUT:
      return canbus::ERROR_NOMSG;
    case ESP_OK:
      break;
    default:
      ESP_LOGE(TAG, "Failed to read: %s", esp_err_to_name(err));
      return canbus::ERROR_FAIL;
  }

  frame->can_id = message.identifier;
  frame->use_extended_id = message.flags & TWAI_MSG_FLAG_EXTD;
  frame->remote_transmission_request = message.flags & TWAI_MSG_FLAG_RTR;
  frame->can_data_length_code = message.data_length_code;

  if (!frame->remote_transmission_request) {
    size_t dlc =
        message.data_length_code < canbus::CAN_MAX_DATA_LENGTH ? message.data_length_code : canbus::CAN_MAX_DATA_LENGTH;
    memcpy(frame->data, message.data, dlc);
  }

  return canbus::ERROR_OK;
}

void ESP32Can::loop() {
  if (this->initialized_g_config_ &&
      0 != memcmp(&*this->initialized_g_config_, &this->g_config_, sizeof(twai_general_config_t))) {
    ESP_LOGI(TAG, "Configuration changed. Reinitializing");
    twai_stop();
    twai_driver_uninstall();

    this->install_and_start_();
  }
#if defined(USE_SENSOR) || defined(USE_TEXT_SENSOR)
  twai_status_info_t status;
  const auto err = twai_get_status_info(&status);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get status info: %s", esp_err_to_name(err));
  } else {
#ifdef USE_SENSOR
    if (this->msgs_to_tx_sensor_ && (!this->prev_status_ || this->prev_status_->msgs_to_tx != status.msgs_to_tx)) {
      this->msgs_to_tx_sensor_->publish_state(status.msgs_to_tx);
    }
    if (this->msgs_to_rx_sensor_ && (!this->prev_status_ || this->prev_status_->msgs_to_rx != status.msgs_to_rx)) {
      this->msgs_to_rx_sensor_->publish_state(status.msgs_to_rx);
    }
    if (this->tx_error_counter_sensor_ &&
        (!this->prev_status_ || this->prev_status_->tx_error_counter != status.tx_error_counter)) {
      this->tx_error_counter_sensor_->publish_state(status.tx_error_counter);
    }
    if (this->rx_error_counter_sensor_ &&
        (!this->prev_status_ || this->prev_status_->rx_error_counter != status.rx_error_counter)) {
      this->rx_error_counter_sensor_->publish_state(status.rx_error_counter);
    }
    if (this->tx_failed_count_sensor_ &&
        (!this->prev_status_ || this->prev_status_->tx_failed_count != status.tx_failed_count)) {
      this->tx_failed_count_sensor_->publish_state(status.tx_failed_count);
    }
    if (this->rx_missed_count_sensor_ &&
        (!this->prev_status_ || this->prev_status_->rx_missed_count != status.rx_missed_count)) {
      this->rx_missed_count_sensor_->publish_state(status.rx_missed_count);
    }
    if (this->rx_overrun_count_sensor_ &&
        (!this->prev_status_ || this->prev_status_->rx_overrun_count != status.rx_overrun_count)) {
      this->rx_overrun_count_sensor_->publish_state(status.rx_overrun_count);
    }
    if (this->arb_lost_count_sensor_ &&
        (!this->prev_status_ || this->prev_status_->arb_lost_count != status.arb_lost_count)) {
      this->arb_lost_count_sensor_->publish_state(status.arb_lost_count);
    }
    if (this->bus_error_count_sensor_ &&
        (!this->prev_status_ || this->prev_status_->bus_error_count != status.bus_error_count)) {
      this->bus_error_count_sensor_->publish_state(status.bus_error_count);
    }
    this->prev_status_ = status;
#endif  // USE_SENSOR
#ifdef USE_TEXT_SENSOR
    if (this->state_text_sensor_ && twai_last_state_ != status.state) {
      twai_last_state_ = status.state;
      this->state_text_sensor_->publish_state([&]() -> const char * {
        switch (status.state) {
          case TWAI_STATE_STOPPED:
            return "Stopped";
          case TWAI_STATE_RUNNING:
            return "Running";
          case TWAI_STATE_BUS_OFF:
            return "Bus Off";
          case TWAI_STATE_RECOVERING:
            return "Recovering";
        }
        return "Unknown";
      }());
    }
#endif  // USE_TEXT_SENSOR
  }
#endif  // defined(USE_SENSOR) || defined(USE_TEXT_SENSOR)

  canbus::Canbus::loop();
}

esp_err_t ESP32Can::start() { return twai_start(); }
esp_err_t ESP32Can::stop() { return twai_stop(); }
esp_err_t ESP32Can::initiate_recovery() { return twai_initiate_recovery(); }

bool ESP32Can::install_and_start_() {
  ESP_LOGV(TAG, "Install TWAI driver");
  this->initialized_g_config_.reset();
  const auto install_err = twai_driver_install(&this->g_config_, &this->t_config_, &this->f_config_);
#ifdef USE_ESP32_CAN_CONFIG_CALLBACK
  config_callback_manager_.call(this->g_config_, this->t_config_, this->f_config_, install_err);
#endif
  switch (install_err) {
    case ESP_OK:
      this->initialized_g_config_ = this->g_config_;
      break;
    default:
      ESP_LOGE(TAG, "Failed to install driver: %s", esp_err_to_name(install_err));
      this->mark_failed();
      return false;
  }

  ESP_LOGV(TAG, "Start TWAI driver");
  const auto start_err = this->start();
  if (start_err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start driver: %s", esp_err_to_name(start_err));
    this->mark_failed();
    return false;
  }

  return true;
}

}  // namespace esp32_can
}  // namespace esphome

#endif
