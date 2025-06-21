#include "hamulight_component.h"
#include "esphome/core/log.h"          // For logging output in the ESPHome log
#include "esphome/core/helpers.h"      // For utility functions like round
#include "esphome/core/hal.h"          // For GPIOPin methods and delayMicroseconds

// --- Added: ESP32 RMT hardware peripheral include for precise RF timing ---
#ifdef USE_ESP32
#include "driver/rmt_tx.h"
#endif

namespace esphome {
namespace hamulight {

// Define a tag for logging output to identify messages from this component
static const char *const TAG = "hamulight";

/**
 * @brief Called once at ESP startup.
 *
 * Initializes the GPIO pins and outputs basic configuration information.
 */
void Hamulight::setup() {
  // Sets the RF transmit pin as an output and ensures it's initially LOW.
  this->rf_transmit_pin_->setup();
  this->rf_transmit_pin_->digital_write(false);

  // If an optional LED pin is configured, initializes it as an output and turns the LED off.
  if (this->led_pin_ != nullptr) {
    this->led_pin_->setup();
    this->led_pin_->digital_write(false);
  }

  // Logs the successful initialization of the component and its configuration.
  ESP_LOGCONFIG(TAG, "Hamulight is being set up...");
  ESP_LOGCONFIG(TAG, "  RF Transmit Pin: configured");
  ESP_LOGCONFIG(TAG, "  RF Address: 0x%04X", this->rf_address_);
  if (this->led_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  LED Pin: configured");
  }
}

/**
 * @brief Outputs the complete configuration of the component to the log.
 *
 * This method is useful for debugging to check if the YAML configuration
 * has been correctly applied by the component.
 */
void Hamulight::dump_config() {
  ESP_LOGCONFIG(TAG, "  Hamulight (RF Light)");
  ESP_LOGCONFIG(TAG, "  RF Transmit Pin: configured");
  if (this->led_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  LED Pin: configured");
  }
  ESP_LOGCONFIG(TAG, "  RF Address: 0x%04X", this->rf_address_);
}

/**
 * @brief Returns the supported features of the light to Home Assistant.
 *
 * This indicates that the light supports on/off and brightness control.
 * Color functions are not relevant in this case.
 * @return light::LightTraits object.
 */
light::LightTraits Hamulight::get_traits() {
  auto traits = light::LightTraits();
  // Unterstützt nur Dimmen (Helligkeit)
  traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
  return traits;
}

/**
 * @brief Called by Home Assistant to change the state of the light (brightness and on/off).
 *
 * Diese Methode ist der zentrale Punkt für alle HA-Interaktionen (Power-Button und Slider).
 * Sie prüft zuerst, ob das Licht ein- oder ausgeschaltet werden soll (is_on).
 * Danach prüft sie auf den gewünschten Helligkeitswert (brightness).
 * Power-Button AUS: RF_POWER_COMMAND
 * Power-Button EIN + Helligkeit < 100%: Sendet Dimmwert
 * Power-Button EIN + Helligkeit == 100%: RF_BRIGHT100_COMMAND (Pairing)
 */
void Hamulight::write_state(light::LightState *state) {
  bool is_on = state->remote_values.is_on();
  float brightness = state->remote_values.get_brightness();

  ESP_LOGD(TAG, "write_state() called, HA is_on: %d, brightness: %.4f", (int)is_on, brightness);

  if (!is_on) {
    ESP_LOGD(TAG, "Sending RF_POWER_COMMAND (OFF)");
    this->transmit_rf_command(RF_POWER_COMMAND);
    return;
  }

  // Jetzt ist das Licht EIN:
  if (brightness >= 0.999f) {
    ESP_LOGD(TAG, "Sending RF_BRIGHT100_COMMAND (pairing, no offset)");
    this->transmit_rf_command(RF_BRIGHT100_COMMAND);
    return;
  }

  // 1. Convert Home Assistant float state (0.0 - 1.0) to a 0-127 range for dimming steps.
  // This provides 128 discrete steps (0 to 127).
  uint8_t dim_value_0_127 = (uint8_t) round(brightness * (RF_SLIDE_STEPS - 1));

  // 2. Calculate the brightness value to transmit.
  // This directly applies the offset and leverages uint8_t's natural overflow behavior
  // as described in the RF protocol.
  // Example: For dim_value_0_127 = 0 (0% brightness), result is 0xA8.
  // Example: For dim_value_0_127 = 88, result is (0xA8 + 88) = 256, which becomes 0x00 as uint8_t.
  uint8_t brightness_to_transmit = RF_SLIDE_OFFSET + dim_value_0_127;

  // A general safeguard: if for some reason the calculated brightness falls outside
  // the expected range, cap it to the maximum allowed RF value.
  // Given the 0-127 input and 0xA8 offset, the values will naturally wrap.
  // The max calculated value is 0xA8 + 127 = 0x127 (295 dec), which wraps to 0x27 (39 dec) as uint8_t.
  // So this check might not be triggered with the current constants, but remains for robustness.
  if (brightness_to_transmit > RF_SLIDE_RANGE_MAX) {
      ESP_LOGW(TAG, "Slider input value (0x%02X) exceeding allowed RF range (0x%02X), capping to max (0x%02X).",
               brightness_to_transmit, RF_SLIDE_RANGE_MAX, RF_SLIDE_RANGE_MAX);
      brightness_to_transmit = RF_SLIDE_RANGE_MAX;
  }

  ESP_LOGD(TAG, "Sending dimming value: %.4f → %d → 0x%02X (with offset)",
           brightness, dim_value_0_127, brightness_to_transmit);
  this->transmit_rf_brightness(brightness_to_transmit);
}

/**
 * @brief Generates the 32-bit signal sequence and fills the code_sequence_ array.
 *
 * This method implements the logic for creating the RF signal,
 * including checksum calculation and bit conversion into pulse durations.
 * @param command The 8-bit command (e.g., 0x5F for Power, or a brightness value).
 */
void Hamulight::generate_code_sequence(uint8_t command) {
  uint32_t combined = 0;
  // Checksum offset as specified in the original code.
  int8_t cks_offset = 83;
  uint8_t checksum = 0;

  // Splits the 16-bit RF address into two 8-bit bytes.
  uint8_t rf_address_byte0 = (uint8_t)(this->rf_address_ >> 8);   // MSB (Most Significant Byte)
  uint8_t rf_address_byte1 = (uint8_t)(this->rf_address_ & 0xFF); // LSB (Least Significant Byte)

  // Calculates the checksum byte.
  checksum = (rf_address_byte0 + rf_address_byte1 + command - cks_offset);

  // Combines all 4 signal bytes into a 32-bit word.
  // The order is: Address Byte 0, Address Byte 1, Command, Checksum.
  combined = ((uint32_t)rf_address_byte0 << 24) |
             ((uint32_t)rf_address_byte1 << 16) |
             ((uint32_t)command << 8) |
             (uint32_t)checksum;

  ESP_LOGD(TAG, "RF Command: 0x%02X, Combined Signal: 0x%08X", command, combined);

  // Parses the combined 32-bit word bit by bit.
  // Bits are read from LSB (Bit 0) to MSB (Bit 31) and written in reverse order
  // into the code_sequence_ array to mirror the transmission order.
  int n = (sizeof(combined) * 8) - 1; // n = 31 for a 32-bit word
  for (int i = 0; i < (sizeof(combined) * 8); i++) {
    bool b = (combined >> i) & 1; // Reads the i-th bit from 'combined'
    // Calculates the position in the code_sequence_ array.
    // (n - i) * 2 reverses the bit order and multiplies by 2, since each bit consists of 2 pulses.
    int current_sequence_pos = (n - i) * 2;
    if (b) { // If the bit is 1, use the BIT1_PULSE values
      code_sequence_[current_sequence_pos] = BIT1_PULSE[0];
      code_sequence_[current_sequence_pos + 1] = BIT1_PULSE[1];
    } else { // If the bit is 0, use the BIT0_PULSE values
      code_sequence_[current_sequence_pos] = BIT0_PULSE[0];
      code_sequence_[current_sequence_pos + 1] = BIT0_PULSE[1];
    }
  }
}

/**
 * @brief Public method for sending a specific RF command.
 *
 * Called by Home Assistant buttons to send predefined commands.
 * @param command The 8-bit command (e.g., RF_POWER_COMMAND, RF_BRIGHT100_COMMAND).
 */
void Hamulight::transmit_rf_command(uint8_t command) {
  this->generate_code_sequence(command); // Generates the sequence for the command
  this->send_rf_signal_rmt();            // --- Changed: Use RMT-based sending on ESP32 ---
}

/**
 * @brief Public method for sending a specific RF brightness value.
 *
 * Called by the write_state method (for the Home Assistant brightness slider).
 * @param brightness_value The 8-bit brightness value (in the range of RF_SLIDE_RANGE_MIN to RF_SLIDE_RANGE_MAX).
 */
void Hamulight::transmit_rf_brightness(uint8_t brightness_value) {
  this->generate_code_sequence(brightness_value); // Generates the sequence for the brightness value
  this->send_rf_signal_rmt();                     // Changed: Use RMT-based sending on ESP32
}

/**
 * @brief Sends the generated RF signal using the ESP32 RMT peripheral (ESP32 only).
 *
 * This implementation leverages the ESP32's RMT hardware for precise and non-blocking transmission.
 * The software-based GPIO/delayMicroseconds loop is replaced by a hardware-timed RMT buffer.
 */
#ifdef USE_ESP32
void Hamulight::send_rf_signal_rmt() {
  // Turns on the optional LED, if configured, as visual feedback.
  if (this->led_pin_ != nullptr) {
    this->led_pin_->digital_write(true); // LED ON
  }

  // Setup and create an RMT TX channel for hardware-timed transmission
  rmt_channel_handle_t channel = nullptr;
  rmt_tx_channel_config_t tx_chan_config = {
    .clk_src = RMT_CLK_SRC_DEFAULT,
    .gpio_num = (gpio_num_t)this->rf_transmit_pin_->get_pin(),
    .mem_block_symbols = 64,  // enough for our signal
    .resolution_hz = 1000000, // 1 MHz for microsecond accuracy
    .trans_queue_depth = 1,
    .flags.io_loop_back = false,
    .flags.io_od_mode = false,
    .flags.invert_out = false,
    .intr_priority = 0,
  };
  rmt_new_tx_channel(&tx_chan_config, &channel);

  // Prepare RMT symbols for the transmission buffer
  std::vector<rmt_symbol_word_t> items;

  // Repeat the entire signal transmission multiple times for robustness.
  for (int i = 0; i < SIGNAL_REPETITIONS; i++) {
    // Transmission of the start sequence:
    for (int j = 0; j < START_SEQUENCE_SIZE; j += 2) {
      rmt_symbol_word_t word = {};
      word.level0 = 1;
      word.duration0 = BASE_PULSE * START_SEQUENCE[j];
      word.level1 = 0;
      word.duration1 = BASE_PULSE * START_SEQUENCE[j + 1];
      items.push_back(word);
    }
    // Transmission of the code sequence:
    for (int k = 0; k < CODE_SEQUENCE_SIZE; k += 2) {
      rmt_symbol_word_t word = {};
      word.level0 = 1;
      word.duration0 = BASE_PULSE * code_sequence_[k];
      word.level1 = 0;
      word.duration1 = BASE_PULSE * code_sequence_[k + 1];
      items.push_back(word);
    }
  }

  // RMT encoder setup (copy encoder for raw buffer)
  rmt_copy_encoder_config_t encoder_cfg = {};
  rmt_encoder_handle_t encoder = nullptr;
  rmt_new_copy_encoder(&encoder_cfg, &encoder);

  // Enable channel
  rmt_enable(channel);

  // Transmit the buffer using RMT hardware
  rmt_transmit_config_t tx_config = {};
  tx_config.loop_count = 0;
  tx_config.flags.eot_level = 0;

  esp_err_t err = rmt_transmit(channel, encoder, items.data(), items.size() * sizeof(rmt_symbol_word_t), &tx_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "RMT transmit failed: %d", err);
    rmt_disable(channel);
    rmt_del_channel(channel);
    if (this->led_pin_ != nullptr) this->led_pin_->digital_write(false);
    return;
  }
  rmt_tx_wait_all_done(channel, -1);

  // Clean up
  rmt_disable(channel);
  rmt_del_channel(channel);

  // Turns off the LED, if configured.
  if (this->led_pin_ != nullptr) {
    this->led_pin_->digital_write(false); // LED OFF
  }
  ESP_LOGD(TAG, "RF signal transmission via RMT completed.");
}
#endif

// --- The old software-based send_rf_signal() is still present for reference or fallback, if needed ---

} // namespace hamulight
} // namespace esphome
