/**
 * @file hamulight_component.cpp
 * @brief Implementation of the HamulightComponent for ESPHome
 *
 * This file implements all logic for controlling Hamulight RF lights:
 *  - Toggle (stateless button)
 *  - Pairing/100% brightness (stateless button)
 *  - Brightness (stateless slider/number)
 *  - Command scanning (start/stop buttons, scan config via numbers, last scanned command via sensor)
 *  - RMT-based RF transmission for ESP32 and variants
 *
 * All configuration and entity wiring is handled in __init__.py.
 *
 * Author: madmat17, 2025/06
 */

#include "hamulight_component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include "esphome/core/application.h"  // <-- Required for 'App'
#include <driver/gpio.h>

namespace esphome {
namespace hamulight {

// Define a tag for logging output to identify messages from this component
static const char *const TAG = "hamulight";

/**
 * @brief Constructor
 *
 * Logs construction for debugging.
 */
HamulightComponent::HamulightComponent() { ESP_LOGCONFIG(TAG, "HamulightComponent CONSTRUCTOR called!"); }

/**
 * @brief ESPHome setup() lifecycle.
 *  - Initialize all configured pins (RF and optional LED)
 *  - Allocate and configure RMT channel and encoder (once, for all RF transmissions)
 *  - Register custom button/number entities for Home Assistant
 */
void HamulightComponent::setup() {
  // Initialize RF output pin, set LOW initially.
  this->rf_transmit_pin_->setup();
  this->rf_transmit_pin_->digital_write(false);

  // Optional feedback LED
  if (this->led_pin_ != nullptr) {
    this->led_pin_->setup();
    this->led_pin_->digital_write(false);
  }

  ESP_LOGCONFIG(TAG, "setup(): HamulightComponent is being set up...");
  ESP_LOGCONFIG(TAG, "  RF Transmit Pin: GPIO%u", this->rf_pin_num_);
  if (this->led_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  LED Pin: GPIO%u", this->led_pin_num_);
  }
  ESP_LOGCONFIG(TAG, "  RF Address: 0x%04X", this->rf_address_);
  ESP_LOGCONFIG(TAG, "  Command Scanner: %s", scanner_running_ ? "ENABLED" : "DISABLED");

#if defined(USE_ESP32) || defined(USE_ESP32_VARIANT) || defined(USE_ESP32S2) || defined(USE_ESP32S3) || \
    defined(USE_ESP32C3)
  // ----------- RMT Peripheral Allocation -----------
  ESP_LOGD(TAG, "setup(): === Entered RMT setup block ===");  // Log -> did this part compile?
  ESP_LOGD(TAG, "setup(): rf_pin_num_ = %u", this->rf_pin_num_);

  bool open_drain = (this->rf_transmit_pin_->get_flags() & gpio::FLAG_OPEN_DRAIN) != 0;
  rmt_tx_channel_config_t channel;
  memset(&channel, 0, sizeof(channel));
  channel.clk_src = RMT_CLK_SRC_DEFAULT;
  channel.resolution_hz = 1000000;  // 1 MHz = 1us resolution
  channel.gpio_num = gpio_num_t(this->rf_pin_num_);
  channel.mem_block_symbols = 64;
  channel.trans_queue_depth = 1;
  channel.flags.io_loop_back = open_drain;
  channel.flags.io_od_mode = open_drain;
  channel.flags.invert_out = 0;
  channel.flags.with_dma = false;
  channel.intr_priority = 0;

  esp_err_t error = rmt_new_tx_channel(&channel, &this->tx_channel_);
  ESP_LOGD(TAG, "setup(): rmt_new_tx_channel returned: %d", error);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "setup(): rmt_new_tx_channel failed: %s", esp_err_to_name(error));
    this->tx_channel_ = nullptr;
    this->encoder_ = nullptr;
    this->mark_failed();
    return;
  }

  // Configure pullup if required
  if (this->rf_transmit_pin_->get_flags() & gpio::FLAG_PULLUP) {
    gpio_pullup_en(gpio_num_t(this->rf_pin_num_));
  } else {
    gpio_pullup_dis(gpio_num_t(this->rf_pin_num_));
  }

  // Allocate encoder (raw copy)
  rmt_copy_encoder_config_t encoder_cfg = {};
  error = rmt_new_copy_encoder(&encoder_cfg, &this->encoder_);
  ESP_LOGD(TAG, "setup(): rmt_new_copy_encoder returned: %d", error);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "setup(): rmt_new_copy_encoder failed: %s", esp_err_to_name(error));
    this->tx_channel_ = nullptr;
    this->encoder_ = nullptr;
    this->mark_failed();
    return;
  }

  // Enable channel
  error = rmt_enable(this->tx_channel_);
  ESP_LOGD(TAG, "setup(): rmt_enable returned: %d", error);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "setup(): rmt_enable failed: %s", esp_err_to_name(error));
    this->tx_channel_ = nullptr;
    this->encoder_ = nullptr;
    this->mark_failed();
    return;
  }

  ESP_LOGD(TAG, "setup(): RMT channel and encoder successfully initialized.");
#endif
}

/**
 * @brief ESPHome dump_config() - logs current configuration.
 *
 * INCOMPLETE, as RF tranmit PIN and LED Pin are not handed over to log print
 * (
 */
void HamulightComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "  HamulightComponent Configuration");
  ESP_LOGCONFIG(TAG, "  RF Transmit Pin: GPIO%u", this->rf_pin_num_);
  if (this->led_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  LED Pin: GPIO%u", this->led_pin_num_);
  }
  ESP_LOGCONFIG(TAG, "  RF Address: 0x%04X", this->rf_address_);
  ESP_LOGCONFIG(TAG, "  Command Scanner: %s", scanner_running_ ? "ENABLED" : "DISABLED");
  if (cmdscan_start_ && cmdscan_end_ && cmdscan_pause_) {
    ESP_LOGCONFIG(TAG, "  Command Scan Range: %u ... %u, Pause: %u ms", static_cast<uint8_t>(cmdscan_start_->state),
                    static_cast<uint8_t>(cmdscan_end_->state), static_cast<uint32_t>(cmdscan_pause_->state));
  } else {
    ESP_LOGCONFIG(TAG, "  Command Scan Range: not fully configured");
  }
}

/**
 * @brief Generates the RF code sequence for a given command.
 *
 *  - Packs address, command, and checksum into 32 bits
 *  - Converts each bit to a pair of pulse entries (Manchester encoding)
 *  - Result: code_sequence_[] is ready for transmission
 *
 * @param command 8-bit command (toggle, pair, brightness, or scanned)
 */
void HamulightComponent::generate_code_sequence(uint8_t command) {
  uint32_t combined = 0;
  int8_t cks_offset = 83;
  uint8_t checksum = 0;

  // Splits the 16-bit RF address into two 8-bit bytes.
  uint8_t rf_address_byte0 = (uint8_t) (this->rf_address_ >> 8);    // MSB (Most Significant Byte)
  uint8_t rf_address_byte1 = (uint8_t) (this->rf_address_ & 0xFF);  // LSB (Least Significant Byte)

  // Calculates the checksum byte.
  checksum = (rf_address_byte0 + rf_address_byte1 + command - cks_offset);

  // Format: [address M] [address L] [command] [checksum]
  combined = ((uint32_t) rf_address_byte0 << 24) | ((uint32_t) rf_address_byte1 << 16) | ((uint32_t) command << 8) |
              (uint32_t) checksum;

  ESP_LOGD(TAG, "RF Command: 0x%02X, Combined Signal: 0x%08X", command, combined);

  // Manchester encode, writing bits in reverse order for transmission
  // Bits are read from LSB (Bit 0) to MSB (Bit 31) and written in reverse order
  // into the code_sequence_ array to mirror the transmission order.
  int n = (sizeof(combined) * 8) - 1;  // n = 31 for a 32-bit word
  for (int i = 0; i < (sizeof(combined) * 8); i++) {
    bool b = (combined >> i) & 1;  // Reads the i-th bit from 'combined'
    // Calculates the position in the code_sequence_ array.
    // (n - i) * 2 reverses the bit order and multiplies by 2, since each bit consists of 2 pulses.
    int current_sequence_pos = (n - i) * 2;
    if (b) {
      // If the bit is 1, use the BIT1_PULSE values
      code_sequence_[current_sequence_pos] = BIT1_PULSE[0];
      code_sequence_[current_sequence_pos + 1] = BIT1_PULSE[1];
    } else {
      // If the bit is 0, use the BIT0_PULSE values
      code_sequence_[current_sequence_pos] = BIT0_PULSE[0];
      code_sequence_[current_sequence_pos + 1] = BIT0_PULSE[1];
    }
  }
}

void HamulightComponent::toggle() {
  // Called from YAML button: Toggle the RF device
  this->transmit_rf_command(RF_POWER_COMMAND);
}

void HamulightComponent::pair_with_driver() {
  // Called from YAML button: Pair with Hamulight driver
  this->transmit_rf_command(RF_BRIGHT100_COMMAND);
}

void HamulightComponent::set_brightness(float brightness) {
  // Called from YAML number: Set brightness (0-100%)
  // Maps 0-100 (float) to RF dimming protocol value, including offset and clamping.
  uint8_t dim_raw = static_cast<uint8_t>(brightness * (RF_SLIDE_STEPS - 1) / 100.0f);
  uint8_t rf_value = RF_SLIDE_OFFSET + dim_raw;
  if (rf_value > RF_SLIDE_RANGE_MAX)
    rf_value = RF_SLIDE_RANGE_MAX;
  this->transmit_rf_brightness(rf_value);
}

void HamulightComponent::start_command_scan() {
  // Called from YAML button: Start command scan
  if (!scanner_running_) {
    scanner_running_ = true;
    scanner_last_time_ = 0;
    scanner_current_ = cmdscan_start_ ? static_cast<uint8_t>(cmdscan_start_->state) : 0;
    ESP_LOGI(TAG, "Command scan started at command 0x%02X.", scanner_current_);
  }
}

void HamulightComponent::stop_command_scan() {
  // Called from YAML button: Stop command scan
  if (scanner_running_) {
    scanner_running_ = false;
    ESP_LOGI(TAG, "Command scan stopped at command 0x%02X.", scanner_current_);
  }
}

/**
 * @brief ESPHome loop() - handles the command scanner state machine.
 *  - Sends each command in range [start, end], at pause interval.
 *  - Publishes the last scanned command to the template sensor.
 *  - Keeps track of scanner state and publishes progress.
 */
void HamulightComponent::loop() {
  if (!scanner_running_)
    return;

  // Check if all scanner configuration entities are present
  if (!cmdscan_start_ || !cmdscan_end_ || !cmdscan_pause_)
    return;

  uint8_t start = static_cast<uint8_t>(cmdscan_start_->state);
  uint8_t end = static_cast<uint8_t>(cmdscan_end_->state);
  uint32_t pause = static_cast<uint32_t>(cmdscan_pause_->state);

  // On the first run or when scan was just started, reset to start value
  if (scanner_last_time_ == 0)
    scanner_current_ = start;

  uint32_t now = millis();
  if (scanner_last_time_ == 0 || (now - scanner_last_time_ >= pause)) {
    if (scanner_current_ <= end) {
      ESP_LOGD(TAG, "Command scan: sending 0x%02X", scanner_current_);
      this->transmit_rf_command(scanner_current_);
      // Update last scanned command sensor (if assigned)
      if (last_scanned_sensor_ != nullptr)
        last_scanned_sensor_->publish_state(scanner_current_);
      scanner_last_time_ = now;
      scanner_current_++;
    } else {
      scanner_running_ = false;
      ESP_LOGI(TAG, "Command scan finished at command 0x%02X.", scanner_current_ - 1);
      // Optionally, publish a "done" value to the sensor (not required)
    }
  }
}

/**
 * @brief Internal: Transmit an arbitrary 8-bit RF command (used by all actions).
 * @param command 8-bit protocol command
 */
void HamulightComponent::transmit_rf_command(uint8_t command) {
  ESP_LOGD(TAG, "transmit_rf_command: 0x%02X", command);
  this->generate_code_sequence(command);
  this->send_rf_signal_rmt();
}

/**
 * @brief Internal: Transmit a protocol brightness value (as per set_brightness).
 * @param brightness_value 8-bit protocol value
 */
void HamulightComponent::transmit_rf_brightness(uint8_t brightness_value) {
  ESP_LOGD(TAG, "transmit_rf_brightness: 0x%02X", brightness_value);
  this->generate_code_sequence(brightness_value);
  this->send_rf_signal_rmt();
}

#if defined(USE_ESP32) || defined(USE_ESP32_VARIANT) || defined(USE_ESP32S2) || defined(USE_ESP32S3) || \
    defined(USE_ESP32C3)
/**
 * @brief Low-level: Send the code_sequence_[] via ESP32 RMT peripheral.
 *  - Allocates no hardware resources here (all done in setup)
 *  - Handles feedback LED on/off
 *  - Sends start sequence + code sequence, repeated SIGNAL_REPETITIONS times
 */
void HamulightComponent::send_rf_signal_rmt() {
  if (this->led_pin_ != nullptr) {
    this->led_pin_->digital_write(true);
  }

  if (this->tx_channel_ == nullptr || this->encoder_ == nullptr) {
    ESP_LOGE(TAG, "RMT channel or encoder not initialized!");
    if (this->led_pin_ != nullptr)
      this->led_pin_->digital_write(false);
    return;
  }

  ESP_LOGD(TAG, "Preparing RMT items buffer...");

  std::vector<rmt_symbol_word_t> items;

  // Compose the full RF transmission: START_SEQUENCE and code_sequence_
  // Repeat the transmission for protocol robustness - probalby to be dismissed due to HA sending command repeatedly
  for (int i = 0; i < SIGNAL_REPETITIONS; i++) {
    // Start sequence
    for (int j = 0; j < START_SEQUENCE_SIZE; j += 2) {
      rmt_symbol_word_t word = {};
      word.level0 = 1;
      word.duration0 = BASE_PULSE * START_SEQUENCE[j];
      word.level1 = 0;
      word.duration1 = BASE_PULSE * START_SEQUENCE[j + 1];
      items.push_back(word);
    }
    // Code sequence (payload)
    for (int k = 0; k < CODE_SEQUENCE_SIZE; k += 2) {
      rmt_symbol_word_t word = {};
      word.level0 = 1;
      word.duration0 = BASE_PULSE * code_sequence_[k];
      word.level1 = 0;
      word.duration1 = BASE_PULSE * code_sequence_[k + 1];
      items.push_back(word);
    }
  }

  ESP_LOGD(TAG, "Starting RMT transmission: total items = %d", static_cast<int>(items.size()));

  rmt_transmit_config_t tx_config;
  memset(&tx_config, 0, sizeof(tx_config));
  tx_config.loop_count = 0;
  tx_config.flags.eot_level = 0;

  esp_err_t error = rmt_transmit(this->tx_channel_, this->encoder_, items.data(),
                                 items.size() * sizeof(rmt_symbol_word_t), &tx_config);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "RMT transmit failed: %s", esp_err_to_name(error));
    if (this->led_pin_ != nullptr)
      this->led_pin_->digital_write(false);
    return;
  }
  rmt_tx_wait_all_done(this->tx_channel_, -1);

  ESP_LOGD(TAG, "RF signal transmission via RMT completed.");

  if (this->led_pin_ != nullptr) {
    this->led_pin_->digital_write(false);
  }
}
#endif

}  // namespace hamulight
}  // namespace esphome
