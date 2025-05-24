#include "hx711.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace hx711 {

static const char *const TAG = "hx711";

static const char *const RETRY_NAME_CHANNEL_B = "channel_b";
static const char *const DEFER_NAME_RESTART = "restart";

/// @brief Converts HX711 gain enum to its corresponding numeric gain value.
/// @param[in] gain Gain setting as an HX711Gain enum.
/// @return Numeric gain value (128, 64, 32, or 0 if invalid).
constexpr uint8_t hx711_gain_to_linear_gain(const HX711Gain gain) {
  return (gain == HX711Gain::HX711_GAIN_128)  ? 128U
         : (gain == HX711Gain::HX711_GAIN_64) ? 64U
         : (gain == HX711Gain::HX711_GAIN_32) ? 32U
                                              : 0U;
}

void HX711Sensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HX711 '%s'...", this->name_.c_str());
  this->sck_pin_->setup();
  this->dout_pin_->setup();

  // Reset the HX711
  this->power_down_internal_();
  delayMicroseconds(60);
  this->power_up();
}

void HX711Sensor::power_down_internal_() {
  // When PD_SCK pin changes from low to high and stays at high for longer than 60µs, HX711 enters power down mode.
  this->sck_pin_->digital_write(true);
  this->settled_ = false;
}

void HX711Sensor::power_up_internal_() {
  // When PD_SCK pin changes from high to low and stays at low, HX711 exits power down mode.
  this->sck_pin_->digital_write(false);
  this->settled_ = false;
}

void HX711Sensor::power_up() {
  if (!this->is_powered_down()) {
    ESP_LOGW(TAG, "HX711 is already powered up.");
    return;
  }

  ESP_LOGI(TAG, "Powering up HX711.");
  this->power_up_internal_();

  // After a reset or power-down event, input selection is default to Channel A with a gain of 128.
  this->last_gain_ = HX711Gain::HX711_GAIN_128;

  if (this->gain_ != this->last_gain_) {
    ESP_LOGD(TAG, "Setting gain to x%u", hx711_gain_to_linear_gain(this->gain_));
  }

  this->set_retry("gain_set", 100, 10, [this](const uint8_t remaining_attempts) {
    // Wait for HX711 to be ready
    if (!this->dout_pin_->digital_read()) {
      if (this->gain_ == HX711Gain::HX711_GAIN_128) {
        this->start_settle_timeout_();
        return RetryResult::DONE;
      }
      // Force a read to set the gain, this will start settle timeout
      if (this->read_sensor_(nullptr, true, true)) {
        return RetryResult::DONE;
      }
    }

    if (remaining_attempts == 0) {
      this->mark_failed("failed to set gain");
    }

    return RetryResult::RETRY;
  });
}

bool HX711Sensor::is_powered_down() const {
  // PD_SCK pin is always left low after reading data.
  return this->sck_pin_->digital_read();
}

void HX711Sensor::power_down(const bool stop_poller) {
  if (this->is_powered_down()) {
    if (!this->power_down_after_reading_) {
      ESP_LOGW(TAG, "HX711 already powered down.");
      return;
    } else {
      ESP_LOGD(TAG, "Disabling wakeup from automatic power-down");
    }
  }

  ESP_LOGI(TAG, "Powering down and canceling timeouts");
  this->cancel_timeout("settling");
  this->cancel_defer(DEFER_NAME_RESTART);
  this->cancel_retry(RETRY_NAME_CHANNEL_B);
  this->cancel_retry("gain_set");
  if (stop_poller) {
    ESP_LOGW(TAG, "Stopping update poller");
    this->stop_poller();
  }
  this->power_down_internal_();
  delayMicroseconds(60);
}

void HX711Sensor::start_settle_timeout_() {
  this->settled_ = false;
  this->stop_poller();

  ESP_LOGD(TAG, "Waiting %u ms for HX711 ADC to settle before next update()", this->settling_time_ms_);
  this->set_timeout("settling", this->settling_time_ms_, [this]() {
    this->settled_ = true;
    this->status_clear_warning();

    ESP_LOGD(TAG, "HX711 ADC settled.");
#if defined(USE_HX711_CHANNEL_B_SENSOR)
    if (!this->channel_b_sensor_read_pending_) {
      this->start_poller();
      return;
    }

    this->channel_b_sensor_read_pending_ = false;
    ESP_LOGV(TAG, "Channel B update scheduled");
    this->set_retry(RETRY_NAME_CHANNEL_B, 100, 10, [this](const uint8_t remaining_attempts) {
      // Wait for HX711 to be ready
      if (!this->dout_pin_->digital_read()) {
        ESP_LOGD(TAG, "Reading channel b");
        uint32_t result;

        // Do not start settle timeout
        bool read_operation_result = this->read_sensor_(&result, false);

        if (this->power_down_after_reading_) {
          // Power-down after channel B read if needed
          this->power_down(false);
        } else {
          this->defer("gain_restore_settle", [this]() { this->start_settle_timeout_(); });
        }

        if (read_operation_result) {
          this->log_and_publish_channel_b_value_(static_cast<int32_t>(result));
        }

        this->start_poller();

        return RetryResult::DONE;
      }

      if (remaining_attempts == 0) {
        this->status_set_warning("failed to read channel b");
      }

      return RetryResult::RETRY;
    });
#else
    this->start_poller();
#endif
  });
}

void HX711Sensor::set_new_gain(HX711Gain gain) {
  const char *gain_operation_str = this->gain_ == gain ? "is already" : "will be";
  ESP_LOGD(TAG, "Gain %s set to x%u", gain_operation_str, hx711_gain_to_linear_gain(gain));
  this->set_gain(gain);
}

void HX711Sensor::dump_config() {
  LOG_SENSOR("", "HX711", this);
  LOG_PIN("  DOUT Pin: ", this->dout_pin_);
  LOG_PIN("  SCK Pin: ", this->sck_pin_);
  ESP_LOGCONFIG(TAG, "  Gain: x%u", hx711_gain_to_linear_gain(this->gain_));
  ESP_LOGCONFIG(TAG, "  Last gain: x%u", hx711_gain_to_linear_gain(this->last_gain_));
  ESP_LOGCONFIG(TAG, "  Settling time: %u ms", this->settling_time_ms_);
  ESP_LOGCONFIG(TAG, "  Power-down after reading: %s", YESNO(this->power_down_after_reading_));
  LOG_UPDATE_INTERVAL(this);
}

void HX711Sensor::update() {
  if (this->is_powered_down()) {
    if (!this->power_down_after_reading_) {
      ESP_LOGW(TAG, "HX711 is powered down");
      return;
    }

    ESP_LOGV(TAG, "Powering up HX711 before reading.");
    this->stop_poller();  // Poller will be restarted after power-on settling
    this->power_up();
    // todo instead of waiting for pooller to call update after powerup, call it manually
    return;
  }

  if (this->gain_ != this->last_gain_) {
    ESP_LOGV(TAG, "Setting gain before reading");
    this->stop_poller();
    // Force a read to set the gain, this will start settle timeout
    this->read_sensor_(nullptr, true, true);
    // todo the same as above
    return;
  }

  if (!this->is_settled()) {
    ESP_LOGW(TAG, "HX711 not settled yet.");
    this->status_set_warning("not settled");
    return;
  }

  bool start_settle_timeout_after_read = !this->power_down_after_reading_;

#if defined(USE_HX711_CHANNEL_B_SENSOR)
  bool current_measurement_is_channel_b = this->last_gain_ == HX711Gain::HX711_GAIN_32;
  HX711Gain gain_to_restore = this->last_gain_;

  // If needed, after reading the value, set the channel to channel B
  if (!current_measurement_is_channel_b) {
    ESP_LOGD(TAG, "Channel B reading pending");
    start_settle_timeout_after_read = true;
    this->gain_ = HX711Gain::HX711_GAIN_32;
    this->channel_b_sensor_read_pending_ = true;
  }
#endif

  // Read the sensor
  uint32_t result;
  if (this->read_sensor_(&result, start_settle_timeout_after_read)) {
    int32_t value = static_cast<int32_t>(result);

#if defined(USE_HX711_CHANNEL_B_SENSOR)
    if (current_measurement_is_channel_b) {
      // Since current measurement is from channel b, publish it.
      this->log_and_publish_channel_b_value_(value);
    } else {
      // Pause poller until channel B reading is done.
      this->stop_poller();
      this->gain_ = gain_to_restore;
    }
#endif

    ESP_LOGD(TAG, "'%s': Got value %" PRId32, this->name_.c_str(), value);
    this->publish_state(value);
  }

  if (this->power_down_after_reading_) {
#if defined(USE_HX711_CHANNEL_B_SENSOR)
    // Power down only if there is no pending reading
    if (!this->channel_b_sensor_read_pending_) {
      this->power_down(false);
    }
#else
    // Power down but don't stop the poller
    this->power_down(false);
#endif
  }
}

#if defined(USE_HX711_CHANNEL_B_SENSOR)
void HX711Sensor::log_and_publish_channel_b_value_(const int32_t value) {
  if (this->channel_b_sensor_ == nullptr) {
    ESP_LOGE(TAG, "Channel B sensor does not exist");
    return;
  }
  ESP_LOGD(TAG, "'%s': Got Channel B value %" PRId32, this->channel_b_sensor_->get_name().c_str(), value);
  this->channel_b_sensor_->publish_state(value);
}
#endif

bool HX711Sensor::read_sensor_(uint32_t *result, const bool start_settle_timeout, const bool force) {
  if (this->is_powered_down()) {
    ESP_LOGE(TAG, "HX711 is powered down");
    return false;
  }

  if (this->dout_pin_->digital_read()) {
    ESP_LOGW(TAG, "HX711 is not ready for new measurements yet!");
    this->status_set_warning("not ready");
    return false;
  }

  if (!this->is_settled() && !force) {
    ESP_LOGW(TAG, "Cannot read sensor before it is settled.");
    this->status_set_warning("not settled");
    return false;
  }

  uint32_t data = 0;
  bool final_dout;

  {
    InterruptLock lock;
    for (uint8_t i = 0; i < 24; i++) {
      this->sck_pin_->digital_write(true);
      delayMicroseconds(1);
      data |= uint32_t(this->dout_pin_->digital_read()) << (23 - i);
      this->sck_pin_->digital_write(false);
      delayMicroseconds(1);
    }

    // Cycle clock pin for gain setting
    for (uint8_t i = 0; i < static_cast<uint8_t>(this->gain_); i++) {
      this->sck_pin_->digital_write(true);
      delayMicroseconds(1);
      this->sck_pin_->digital_write(false);
      delayMicroseconds(1);
    }
    final_dout = this->dout_pin_->digital_read();
  }

  bool should_start_settle_timeout = false;

  if ((this->last_gain_ != this->gain_) || force) {
    ESP_LOGD(TAG, "HX711 gain changed from x%u to x%u", hx711_gain_to_linear_gain(this->last_gain_),
             hx711_gain_to_linear_gain(this->gain_));
    this->last_gain_ = this->gain_;
    this->settled_ = false;
    should_start_settle_timeout = start_settle_timeout || force;
  }

  if (!final_dout) {
    ESP_LOGW(TAG, "HX711 DOUT pin not high after reading (data 0x%08" PRIx32 ")! Restarting", data);
    this->status_set_warning("dout not high");
    this->power_down(false);
    this->defer(DEFER_NAME_RESTART, [this]() { this->power_up(); });
    return false;
  }

  if (should_start_settle_timeout) {
    this->start_settle_timeout_();
  }

  if (start_settle_timeout && this->is_settled()) {
    this->status_clear_warning();
  }

  if (result != nullptr) {
    if (data & 0x800000ULL) {
      data |= 0xFF000000ULL;
    }
    *result = data;
  }

  return true;
}

}  // namespace hx711
}  // namespace esphome
