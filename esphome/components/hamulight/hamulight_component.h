#pragma once

/**
 * @file hamulight_component.h
 * @brief Hamulight RF Remote Control Component for ESPHome
 *
 * This component is responsible for controlling Hamulight RF lights using the ESP32 RMT peripheral for
 * precise RF timing. It supports toggle, brightness, pairing, and command scan via Home Assistant integration,
 * with stateless Home Assistant entities (buttons and sliders) for robust, real-world operation.
 *
 * Please note that the command scan feature is experimental and for advanced users, who would like to
 * use this component as basis for their own RF remote control implementation.
 * I used the command scanner for finding commands, that were not available on my physical RF remote control
 * (like seperate commands for on and off) - unfortunately no 'new' commands were discovered.
 * 
 * Main features:
 *  - Uses RMT hardware for non-blocking, accurate RF signal generation.
 *  - Allocates the RMT channel and encoder ONCE in setup() for efficient operation.
 *  - Optional: Feedback LED
 *  - Exposes public methods for toggling, brightness, pairing, and command scanning, allowing all conversions to be handled in C++.
 *
 * Configuration parameters:
 *  - rf_transmit_pin: GPIOPin* for digital pin control and flag checks.
 *  - rf_pin_num: uint8_t for the raw GPIO number used by RMT hardware.
 *  - rf_address: 16-bit address for RF protocol.
 *  - OPTIONAL: led_pin: Optional GPIOPin* for visual feedback.
 *  - OPTIONAL: cmdscan_start_/end_/pause_: pointers to number entities for command scan control.
 *  - OPTIONAL: last_scanned_sensor_: pointer to a template sensor showing the last scanned command.
 * 
 * Author: madmat17
 */

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/number/number.h"
#include "esphome/components/sensor/sensor.h"
#include "hamulight_button.h"
#include "hamulight_number.h"

// IMPORTANT: Use the following for ESP32 and all ESP32 variants (includes ESP32-S2/S3/C3)
#if defined(USE_ESP32) || defined(USE_ESP32_VARIANT) || defined(USE_ESP32S2) || defined(USE_ESP32S3) || defined(USE_ESP32C3)
#include "driver/rmt_tx.h"
#endif

namespace esphome {
namespace hamulight {

// RF command bytes (adjust as per your protocol/spec)
constexpr uint8_t RF_POWER_COMMAND       = 0x5F;   // Command to toggle light ON/OFF
constexpr uint8_t RF_BRIGHT100_COMMAND   = 0x59;   // Command for 100% brightness/pairing
// constexpr uint8_t RF_BRIGHT75_COMMAND = 0x50;   // Command for  75% brightness - Kept for reference / future extensions
// constexpr uint8_t RF_BRIGHT50_COMMAND = 0x56;   // Command for  50% brightness - Kept for reference / future extensions
// constexpr uint8_t RF_BRIGHT25_COMMAND = 0x55;   // Command for  25% brightness - Kept for reference / future extensions

// Brightness/dimming protocol
constexpr uint8_t RF_SLIDE_STEPS        = 128;    // Number of dimming steps (0..127)
constexpr uint8_t RF_SLIDE_OFFSET       = 0xA8;   // Offset for dimming value (protocol-specific)
constexpr uint8_t RF_SLIDE_RANGE_MIN    = 0x80;   // Minimum RF value for dimming (protocol-specific)
constexpr uint8_t RF_SLIDE_RANGE_MAX    = 0xFF;   // Maximum RF value for dimming (protocol-specific)

// PHYSICAL SIGNAL
// Bit encoding for RF protocol (Manchester or similar)
constexpr uint16_t BASE_PULSE           = 200;                                  // Base pulse length in micro seconds
constexpr uint8_t START_SEQUENCE_SIZE   = 10;                                   // Number of pulses during start sequence
constexpr uint16_t START_SEQUENCE[]     = { 1, 1, 1, 1, 1, 1, 1, 1, 6, 6 };     // Pulse sequence for sync + RF start bit (
constexpr uint16_t BIT0_PULSE[]         = { 3, 1 };                             // RF '0': 3T high, 1T low
constexpr uint16_t BIT1_PULSE[]         = { 1, 3 };                             // RF '1': 1T high, 3T low
constexpr uint8_t SIGNAL_REPETITIONS    = 6;                                    // Number of times to repeat the signal
constexpr uint8_t CODE_SEQUENCE_SIZE    = 64;                                   // Code sequence size (4 bytes * 8 bits => 32 bits * 2 pulses per bit)

// --- End Hamulight RF Protocol Constants ---


/**
 * @class HamulightComponent
 * @brief Main Hamulight component class derived from LightOutput and Component.
 *
 * Exposes setup, dump_config, and public toggle/brightness/pair/command scan methods for ESPHome, handles RMT signal generation and sending.
 */
class HamulightComponent : public Component {
 public:
  /**
   * @brief Constructor
   */
  HamulightComponent();

  /**
   * @brief Set the GPIOPin object for RF transmission (for digitalWrite and flag checks).
   * @param pin GPIOPin* object
   */
  void set_rf_transmit_pin(GPIOPin *pin) { this->rf_transmit_pin_ = pin; }

  /**
   * @brief Set the raw GPIO number (for RMT hardware, extracted from YAML).
   * @param pin GPIO number as uint8_t
   */
  void set_rf_pin_num(uint8_t pin) { this->rf_pin_num_ = pin; }

  /**
   * @brief Set the optional feedback LED GPIOPin object and store the pin as number.
   * @param pin GPIOPin* object
   */
  void set_led_pin(GPIOPin *pin) { this->led_pin_ = pin; }
  void set_led_pin_num(uint8_t pin) { this->led_pin_num_ = pin; }

  /**
   * @brief Set the RF address for the protocol.
   * @param address 16-bit RF address
   */
  void set_rf_address(uint16_t address) { this->rf_address_ = address; }

  /**
   * @brief Set number entity for command scan start value.              
   */
  void set_cmdscan_start_number(esphome::number::Number *n) { cmdscan_start_ = n; }

  /**
   * @brief Set number entity for command scan end value.
   */
  void set_cmdscan_end_number(esphome::number::Number *n) { cmdscan_end_ = n; }

  /**
   * @brief Set number entity for command scan pause value.
   */
  void set_cmdscan_pause_number(esphome::number::Number *n) { cmdscan_pause_ = n; }

  /**
   * @brief Set sensor to publish the last scanned command.
   */
  void set_last_scanned_sensor(esphome::sensor::Sensor *s) { last_scanned_sensor_ = s; }

  /**
   * @brief Expose pointers for custom HA YAML automations if needed.
   */
  HamulightButton *get_toggle_button() { return toggle_button_; }
  HamulightButton *get_pair_button() { return pair_button_; }
  HamulightBrightnessNumber *get_brightness_number() { return brightness_number_; }

  /**
   * @brief ESPHome setup lifecycle method. Initializes pins and allocates RMT resources.
   */
  void setup() override;

  /**
   * @brief ESPHome loop method. Handles command scan.
   */
  void loop() override;

  /**
   * @brief Dumps YAML configuration and component status to ESPHome log.
   */
  void dump_config() override;

  /**
   * @brief Public method for sending the RF toggle command (stateless button).
   */
  void toggle();

  /**
   * @brief Public method for sending the RF pair/maximum brightness command (stateless button).
   */
  void pair_with_driver();

  /**
   * @brief Public method for sending a specific RF brightness value (stateless slider).
   * @param brightness 0.0-100.0 float (slider value from Home Assistant)
   */
  void set_brightness(float brightness);

  /**
   * @brief Starts the command scan (from button).
   */
  void start_command_scan();

  /**
   * @brief Stops the command scan (from button).
   */
  void stop_command_scan();

 protected:
  // --- Configuration fields (from YAML/codegen) ---
  GPIOPin *rf_transmit_pin_;  ///< GPIOPin for digital control and flag checks (not used directly in RMT)
  uint8_t rf_pin_num_{255};   ///< Raw GPIO number for RMT hardware peripheral
  GPIOPin *led_pin_{nullptr}; ///< Optional feedback LED
  uint8_t led_pin_num_{255};  ///< Optional feedback LED's pin number
  uint16_t rf_address_;       ///< RF Address for protocol

  // --- Command scanner number pointers ---
  esphome::number::Number *cmdscan_start_{nullptr};
  esphome::number::Number *cmdscan_end_{nullptr};
  esphome::number::Number *cmdscan_pause_{nullptr};

  // --- Sensor for last command scanned ---
  esphome::sensor::Sensor *last_scanned_sensor_{nullptr};

  // --- Custom button and number entities ---
  HamulightButton *toggle_button_{nullptr};
  HamulightButton *pair_button_{nullptr};
  HamulightBrightnessNumber *brightness_number_{nullptr};

#if defined(USE_ESP32) || defined(USE_ESP32_VARIANT) || defined(USE_ESP32S2) || defined(USE_ESP32S3) || defined(USE_ESP32C3)
  // --- RMT hardware handles (allocated ONCE in setup and reused for all transmissions) ---
  rmt_channel_handle_t tx_channel_{nullptr};   ///< RMT TX channel handle for hardware-timed transmission
  rmt_encoder_handle_t encoder_{nullptr};      ///< RMT encoder handle for raw buffer
#endif

  /**
   * @brief Generates the internal code_sequence_ RF buffer from command byte.
   * @param command The 8-bit command or brightness value
   */
  void generate_code_sequence(uint8_t command);

  /**
   * @brief Sends the generated RF signal using the RMT peripheral (ESP32 only).
   * Allocates no hardware resources; uses the handles from setup().
   */
  void send_rf_signal_rmt();

  /**
   * @brief Public method for sending a specific RF command.
   * @param command The 8-bit command (e.g., RF_POWER_COMMAND).
   */
  void transmit_rf_command(uint8_t command);

  /**
   * @brief Public method for sending a specific RF brightness value.
   * @param brightness_value The 8-bit brightness value (in the range of RF_SLIDE_RANGE_MIN to RF_SLIDE_RANGE_MAX).
   */
  void transmit_rf_brightness(uint8_t brightness_value);

  // --- Internal RF signal buffer (holds the 32-bit RF payload as pulse durations) ---
  uint16_t code_sequence_[CODE_SEQUENCE_SIZE];

  // --- Command scanner state ---
  bool scanner_running_{false};
  uint8_t scanner_current_{0};
  uint32_t scanner_last_time_{0};
};

} // namespace hamulight
} // namespace esphome
