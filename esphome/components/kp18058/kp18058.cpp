#include "kp18058.h"
#include "kp18058_cfg.h"

namespace esphome {
namespace kp18058 {

static const char *const TAG = "KP18058";
static const uint8_t I2C_MAX_RETRY = 3;
#define BIT_CHECK(PIN, N) !!(((PIN) & (1 << (N))))

uint8_t get_parity_bit(uint8_t b) {
  uint8_t sum = 0;
  for (int i = 1; i < 8; i++) {
    if (BIT_CHECK(b, i)) {
      sum++;
    }
  }
  return sum % 2;  // 0 for even, 1 for odd
}

KP18058::KP18058() : max_cw_current_(0), max_rgb_current_(0), i2c_ready_(false) {
  for (auto &channel : channels_) {
    channel = nullptr;
  }
}

void KP18058::setup() {
  i2c_.setup();
  i2c_ready_ = i2c_.reset();
}

void KP18058::dump_config() {
  ESP_LOGCONFIG(TAG, "KP18058 LED Driver:");
  LOG_PIN("  Data Pin: ", i2c_.get_data_pin());
  LOG_PIN("  Clock Pin: ", i2c_.get_clock_pin());
  ESP_LOGCONFIG(TAG, "  I2C Communication %s", i2c_ready_ ? "Initialized" : "FAILED");
  ESP_LOGCONFIG(TAG, "  CW max current: %.1f", this->max_cw_current_);
  ESP_LOGCONFIG(TAG, "  RGB max current: %.1f", this->max_rgb_current_);
}

void KP18058::program_led_driver() {
  if (!i2c_ready_) {
    ESP_LOGI(TAG, "Reestablishing communication with KP18058.");
    i2c_ready_ = i2c_.reset();
    if (!i2c_ready_) {
      ESP_LOGE(TAG, "KP18058 I2C Reset failed.");
      return;
    }
  }

  // Returns true if all channels are zero or nullptr
  auto all_channels_zero = [this]() {
    for (auto *channel : channels_) {
      if (channel != nullptr && channel->get_value() > 0) {
        // If any channel is non-zero, return false
        return false;
      }
    }
    return true;
  };

  // Create the settings struct
  KP18058_Settings settings{};

  settings.address_identification = 1;
  settings.working_mode = all_channels_zero() ? STANDBY_MODE : RGBCW_MODE;
  // Set byte address start. valid values are 0 - 13
  // In this message always all bytes are transmitted (starting from 0)
  settings.start_byte_address = 0x00;

  // Set Line Compensation Mechanism
  settings.line_compensation_enable = LC_DISABLE;
  settings.line_comp_threshold = LC_THRESHOLD_260V;
  settings.line_comp_slope = LC_SLOPE_10_PERCENT;
  settings.rc_filter_enable = RC_FILTER_DISABLE;

  // Set max current values
  settings.max_current_out4_5 = static_cast<uint8_t>(max_cw_current_ / 2.5) & 0x1F;
  settings.max_current_out1_3 = static_cast<uint8_t>(max_rgb_current_ / 2.5) & 0x1F;

  // Set dimming method for RGB channels and chop dimming frequency
  settings.chop_dimming_out1_3 = ANALOG_DIMMING;
  settings.chop_dimming_frequency = CD_FREQUENCY_500HZ;

  // Set grayscale values for each output channel
  for (int i = 0; i < 5; i++) {
    uint16_t use_val = (channels_[i] != nullptr) ? channels_[i]->get_value() : 0;
    settings.channels[i].upper_grayscale = (use_val >> 5) & 0x1F;
    settings.channels[i].lower_grayscale = use_val & 0x1F;
  }

  // Calculate parity bits for each byte
  uint8_t *settings_bytes = reinterpret_cast<uint8_t *>(&settings);
  for (size_t i = 0; i < sizeof(KP18058_Settings); i++) {
    settings_bytes[i] |= get_parity_bit(settings_bytes[i]);
  }

  // Send the I2C message
  i2c_.start();
  for (size_t i = 0; i < sizeof(KP18058_Settings); i++) {
    // On error, try to repeat the byte transmission I2C_MAX_RETRY times
    bool write_succeeded;
    for (int attempt = 0; attempt < I2C_MAX_RETRY; attempt++) {
      write_succeeded = i2c_.write_byte(settings_bytes[i]);
      if (write_succeeded)
        break;
    }
    // If all tries failed, break and stop sending the rest of the frame bytes
    if (!write_succeeded) {
      ESP_LOGE(TAG, "Failed to write byte %02d (0x%02X) after %d attempts", i, settings_bytes[i], I2C_MAX_RETRY);
      i2c_ready_ = false;
      break;
    }
  }
  i2c_.stop();
}

}  // namespace kp18058
}  // namespace esphome
