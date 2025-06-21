#pragma once

/**
 * @file hamulight_component.h
 * @brief Hamulight Light Output Component for ESPHome
 *
 * This component is responsible for controlling Hamulight RF lights using the ESP32 RMT peripheral for
 * precise RF timing. It supports on/off and brightness control via Home Assistant integration.
 * 
 * Main features:
 *  - Uses RMT hardware for non-blocking, accurate RF signal generation.
 *  - Allocates the RMT channel and encoder ONCE in setup() for efficient operation.
 *  - Optionally supports a feedback LED.
 *
 * Configuration parameters:
 *  - rf_transmit_pin: GPIOPin* for digital pin control and flag checks.
 *  - rf_pin_num: uint8_t for the raw GPIO number used by RMT hardware.
 *  - led_pin: Optional GPIOPin* for visual feedback.
 *  - rf_address: 16-bit address for RF protocol.
 * 
 * Author: madmat17
 */

#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"
#include "esphome/core/hal.h"

// IMPORTANT: Use the following for ESP32 and all ESP32 variants (includes ESP32-S2/S3/C3)
#if defined(USE_ESP32) || defined(USE_ESP32_VARIANT) || defined(USE_ESP32S2) || defined(USE_ESP32S3) || defined(USE_ESP32C3)
#include "driver/rmt_tx.h"
#endif

namespace esphome {
namespace hamulight {

// RF command bytes (adjust as per your protocol/spec)
constexpr uint8_t RF_POWER_COMMAND       = 0x5F;   // Command to turn off
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
constexpr uint16_t BASE_PULSE           = 200;                                              // Base pulse length in micro seconds
constexpr uint8_t START_SEQUENCE_SIZE   = 10;                                               // number of pulses during start sequence
constexpr uint16_t START_SEQUENCE[]     = { 1, 1, 1, 1, 1, 1, 1, 1, 6, 6 };                 // pulse sequence for sync + RF start bit (
constexpr uint16_t BIT1_PULSE[]         = { 1, 3 };                                         // RF '1': 1T high, 3T low
constexpr uint16_t BIT0_PULSE[]         = { 3, 1 };                                         // RF '0': 3T high, 1T low
constexpr uint8_t SIGNAL_REPETITIONS    = 6;                                                // Number of times to repeat the signal
constexpr uint8_t CODE_SEQUENCE_SIZE    = 64;                                               // Code sequence size (4 bytes * 8 bits => 32 bits * 2 pulses per bit)

// --- End Hamulight RF Protocol Constants ---

/**
 * @class HamulightComponent
 * @brief Main Hamulight component class derived from LightOutput and Component.
 *
 * Exposes setup, dump_config, and write_state methods for ESPHome, handles RMT signal generation and sending.
 */
class HamulightComponent : public light::LightOutput, public Component {
 public:
  /**
   * @brief Constructor
   *
   * Use this to debug if the constructor is called.
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
   * @brief Set the optional feedback LED GPIOPin object.
   * @param pin GPIOPin* object
   */
  void set_led_pin(GPIOPin *pin) { this->led_pin_ = pin; }

  /**
   * @brief Set the RF address for the protocol.
   * @param address 16-bit RF address
   */
  void set_rf_address(uint16_t address) { this->rf_address_ = address; }

  /**
   * @brief ESPHome setup lifecycle method. Initializes pins and allocates RMT resources.
   */
  void setup() override;

  /**
   * @brief Dumps YAML configuration and component status to ESPHome log.
   */
  void dump_config() override;

  /**
   * @brief Returns supported light traits (on/off, brightness).
   */
  light::LightTraits get_traits() override;

  /**
   * @brief Handles incoming state changes from Home Assistant (on/off, brightness).
   */
  void write_state(light::LightState *state) override;

  /**
   * @brief Transmit a specific RF command.
   * @param command The 8-bit command (e.g. RF_POWER_COMMAND)
   */
  void transmit_rf_command(uint8_t command);

  /**
   * @brief Transmit a specific RF brightness value.
   * @param brightness_value Brightness value (8-bit)
   */
  void transmit_rf_brightness(uint8_t brightness_value);

 protected:
  // --- Configuration fields (from YAML/codegen) ---
  GPIOPin *rf_transmit_pin_;  ///< GPIOPin for digital control and flag checks (not used directly in RMT)
  uint8_t rf_pin_num_{255};   ///< Raw GPIO number for RMT hardware peripheral
  GPIOPin *led_pin_{nullptr}; ///< Optional feedback LED
  uint16_t rf_address_;       ///< RF Address for protocol

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

  // --- Internal RF signal buffer (holds the 32-bit RF payload as pulse durations) ---
  uint16_t code_sequence_[CODE_SEQUENCE_SIZE];
};

} // namespace hamulight
} // namespace esphome
