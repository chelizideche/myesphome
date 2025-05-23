#include "hx711.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace hx711 {

static const char *const TAG = "hx711";

/// @brief Converts HX711 gain enum to its corresponding numeric gain value.
/// @param[in] gain Gain setting as an HX711Gain enum.
/// @return Numeric gain value (128, 64, 32, or 0 if invalid).
constexpr uint8_t hx711_gain_to_linear_gain(const HX711Gain gain) {
  return (gain == HX711Gain::HX711_GAIN_128)  ? 128U
         : (gain == HX711Gain::HX711_GAIN_64) ? 64U
         : (gain == HX711Gain::HX711_GAIN_32) ? 32U
                                              : 0U;
}

void HX711Sensor::call_setup() {
  this->setup();

  if (this->settle_on_boot_) {
    this->start_settle_timeout_();
  } else {
    // Assume the sensor is already settled
    this->settled_ = true;
    this->start_poller();
  }
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
  if (this->gain_ == HX711Gain::HX711_GAIN_128) {
    this->start_settle_timeout_();
    return;
  }

  ESP_LOGD(TAG, "Setting HX711 gain to x%u", this->gain_);

  // Force read sensor once without publishing to set the gain, this will then start the settling timeout sequence
  this->read_sensor_(nullptr, true);
}

bool HX711Sensor::is_powered_down() const {
  // PD_SCK pin is always left low after reading data.
  return this->sck_pin_->digital_read();
}

void HX711Sensor::power_down(const bool stop_poller) {
  if (this->is_powered_down()) {
    ESP_LOGW(TAG, "HX711 is already powered down.");
    return;
  }

  ESP_LOGI(TAG, "Powering down HX711.");
  this->cancel_timeout("settling");
  if (stop_poller) {
    this->stop_poller();
  }
  this->power_down_internal_();
  delayMicroseconds(60);
}

void HX711Sensor::start_settle_timeout_() {
  this->settled_ = false;
  this->stop_poller();

  ESP_LOGD(TAG, "HX711 is settling for %u ms", this->settling_time_ms_);
  this->set_timeout("settling", this->settling_time_ms_, [this]() {
    this->settled_ = true;
    this->status_clear_warning();

    ESP_LOGD(TAG, "HX711 settled.");
    this->start_poller();
  });
}

void HX711Sensor::dump_config() {
  LOG_SENSOR("", "HX711", this);
  LOG_PIN("  DOUT Pin: ", this->dout_pin_);
  LOG_PIN("  SCK Pin: ", this->sck_pin_);
  ESP_LOGCONFIG(TAG, "  Gain: x%u", hx711_gain_to_linear_gain(this->gain_));
  ESP_LOGCONFIG(TAG, "  Settling time: %u ms", this->settling_time_ms_);
  ESP_LOGCONFIG(TAG, "  Power-down after reading: %s", YESNO(this->power_down_after_reading_));
  LOG_UPDATE_INTERVAL(this);
}
float HX711Sensor::get_setup_priority() const { return setup_priority::DATA; }
void HX711Sensor::update() {
  if (this->is_powered_down()) {
    if (!this->power_down_after_reading_) {
      ESP_LOGW(TAG, "HX711 is powered down, skipping update.");
      return;
    }

    ESP_LOGD(TAG, "Powering up HX711 before reading.");
    this->stop_poller();  // Poller will be restarted after settling
    this->power_up();
    return;
  }

  if (!this->is_settled()) {
    ESP_LOGW(TAG, "HX711 not settled, skipping update.");
    this->status_set_warning("not settled");
    return;
  }

  uint32_t result;
  if (this->read_sensor_(&result)) {
    int32_t value = static_cast<int32_t>(result);
    ESP_LOGD(TAG, "'%s': Got value %" PRId32, this->name_.c_str(), value);
    this->publish_state(value);
  }
}
bool HX711Sensor::read_sensor_(uint32_t *result, const bool force) {
  if (this->is_powered_down()) {
    ESP_LOGE(TAG, "HX711 is powered down, cannot read.");
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

  // After a reset or power-down event, input selection defaults to Channel A with a gain of 128.
  static HX711Gain last_gain = HX711Gain::HX711_GAIN_128;
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

  if (last_gain != this->gain_) {
    ESP_LOGD(TAG, "HX711 gain changed from x%u to x%u", hx711_gain_to_linear_gain(last_gain),
             hx711_gain_to_linear_gain(this->gain_));
    last_gain = this->gain_;

    // If powering down after reading is not enabled, start the settling timeout sequence
    // Force reading will force the settling timeout sequence
    if (!this->power_down_after_reading_ || force) {
      this->start_settle_timeout_();
    }
  }

  // HX711 will be powered down after reading only if reading is not forced
  if (this->power_down_after_reading_ && !force) {
    // Don't stop the poller here
    this->power_down(false);
  }

  if (!final_dout) {
    ESP_LOGW(TAG, "HX711 DOUT pin not high after reading (data 0x%08" PRIx32 ")!", data);
    this->status_set_warning("dout not high");
    return false;
  }

  if (!this->power_down_after_reading_ && this->is_settled()) {
    this->status_clear_warning();
  }

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
