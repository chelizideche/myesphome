#include "hx711.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace hx711 {

static const char *const TAG = "hx711";

/// @brief Converts HX711 gain enum to its corresponding numeric gain value.
/// @param[in] gain HX711 gain setting as an enum.
/// @return Numeric gain value (128, 64, 32, or 0 if invalid).
constexpr uint8_t hx711_gain_to_linear_gain(const HX711Gain gain) {
  return (gain == HX711Gain::HX711_GAIN_128)  ? 128U
         : (gain == HX711Gain::HX711_GAIN_64) ? 64U
         : (gain == HX711Gain::HX711_GAIN_32) ? 32U
                                              : 0U;
}

/// @brief Calculates the elapsed time in milliseconds from a given timestamp.
/// @param[in] timestamp The reference timestamp (in millis).
/// @return The elapsed time in milliseconds.
inline uint32_t millis_elapsed_since(const uint32_t timestamp) { return millis() - timestamp; }

void HX711Sensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HX711 '%s'...", this->name_.c_str());
  this->sck_pin_->setup();
  this->dout_pin_->setup();
  this->sck_pin_->digital_write(false);

  // Force read sensor once without publishing to set the gain
  this->read_sensor_(nullptr, true);
  // Make sure that change is timestamped
  this->last_change_ = millis();
}

void HX711Sensor::dump_config() {
  LOG_SENSOR("", "HX711", this);
  LOG_PIN("  DOUT Pin: ", this->dout_pin_);
  LOG_PIN("  SCK Pin: ", this->sck_pin_);
  ESP_LOGCONFIG(TAG, "  Gain: x%" PRIu8, hx711_gain_to_linear_gain(this->gain_));
  ESP_LOGCONFIG(TAG, "  Settling Time: %" PRIu16 " ms", this->settling_time_ms_);
  LOG_UPDATE_INTERVAL(this);
}

float HX711Sensor::get_setup_priority() const { return setup_priority::DATA; }

void HX711Sensor::update() {
  if (millis_elapsed_since(this->last_change_) < static_cast<uint32_t>(this->settling_time_ms_)) {
    uint32_t settling_time_remaining_ms =
        static_cast<uint32_t>(this->settling_time_ms_) - millis_elapsed_since(this->last_change_);
    ESP_LOGW(TAG, "Waiting %" PRIu32 " ms for HX711 to settle before updating, stopping poller.",
             settling_time_remaining_ms);
    this->stop_poller();
    this->status_set_warning();
    this->set_timeout("hx711_settle", settling_time_remaining_ms, [this]() {
      ESP_LOGI(TAG, "HX711 output settled, starting poller.");
      this->start_poller();
      this->status_clear_warning();
    });
    return;
  }

  uint32_t result;
  if (this->read_sensor_(&result)) {
    int32_t value = static_cast<int32_t>(result);
    ESP_LOGD(TAG, "'%s': Got value %" PRId32, this->name_.c_str(), value);
    this->publish_state(value);
  }
  // No need to log warnings here, they're already logged in read_sensor_ everywhere where it returns false
}

bool HX711Sensor::read_sensor_(uint32_t *result, bool force) {
  if (this->dout_pin_->digital_read()) {
    ESP_LOGW(TAG, "HX711 is not ready for new measurements yet!");
    this->status_set_warning();
    return false;
  }

  uint32_t elapsed = millis_elapsed_since(this->last_change_);
  if (!force && (elapsed < static_cast<uint32_t>(this->settling_time_ms_))) {
    ESP_LOGW(TAG, "HX711 output is not settled yet (%" PRIu32 " ms left)", this->settling_time_ms_ - elapsed);
    this->status_set_warning();
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

  if (this->last_gain_ != this->gain_) {
    // Timestamp the change
    this->last_change_ = millis();
    this->last_gain_ = this->gain_;
    ESP_LOGV(TAG, "HX711 gain changed to x%" PRIu8 " at %" PRIu32 " ms", hx711_gain_to_linear_gain(this->gain_),
             this->last_change_);
  }

  if (!final_dout) {
    ESP_LOGW(TAG, "HX711 DOUT pin not high after reading (data 0x%" PRIx32 ")!", data);
    this->status_set_warning();
    return false;
  }

  this->status_clear_warning();

  if (data & 0x800000ULL) {
    data |= 0xFF000000ULL;
  }

  if (result != nullptr) {
    *result = data;
  }
  return true;
}
}  // namespace hx711
}  // namespace esphome
