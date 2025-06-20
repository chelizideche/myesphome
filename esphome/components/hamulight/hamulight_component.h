#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"
#include "esphome/core/hal.h"                         // Für GPIO PIN operations (digitalWrite, delayMicroseconds)

namespace esphome {
namespace hamulight {

/**
 * @brief The Hamulight class implements an RF light control as an ESPHome component.
 *
 * This class inherits from light::LightOutput to appear as a light in Home Assistant,
 * and from Component for basic ESPHome component functionality.
 */

class Hamulight : public light::LightOutput, public Component {
 public:
  /**
   * @brief Sets the GPIO pin connected to the data input of the RF module.
   * @param pin A pointer to the GPIOPin object.
   */
  void set_rf_transmit_pin(GPIOPin *pin) { this->rf_transmit_pin_ = pin; }

  /**
   * @brief Sets the optional GPIO pin for a feedback LED.
   * @param pin A pointer to the GPIOPin object.
   */
  void set_led_pin(GPIOPin *pin) { this->led_pin_ = pin; }

  /**
   * @brief Sets the 2-byte RF address (unique ID of the remote control).
   * @param address The 16-bit RF address.
   * Address: First 2 bytes of the protocol used by Hamulight remotes is the address. The assumption is that you
   * can choose ANY address here, since the transformator/receiver can be trained for any(?) remote w/ that protocol (remove
   * A/C power supply, reconnect to A/C power supply and send the "max brightness" command within the first 10 seconds). Another
   * option is to analyze the remote's signal...
   * Command: Pre-defined. 5 buttons are hard-coded, capacitive touch field for seamless dimming will be calculated (128
   * different steps assumed).
   * Checksum: Calculated automatically. ATTENTION: The checksum algorythm most likely needs to be adopted within the
   * generateCode() function in case this program will be used for anything else than Hamulight receivers.
   */
  void set_rf_address(uint16_t address) { this->rf_address_ = address; }

  // These constants define the specific RF commands and timing parameters.
  const uint8_t RF_POWER_COMMAND = 0x5F;                                        // Power toggle command
  const uint8_t RF_BRIGHT100_COMMAND = 0x59;                                    // 100% brightness - MIGHT BE DISMISSED
  const uint8_t RF_BRIGHT75_COMMAND = 0x50;                                     //  75% brightness - MIGHT BE DISMISSED
  const uint8_t RF_BRIGHT50_COMMAND = 0x56;                                     //  50% brightness - MIGHT BE DISMISSED
  const uint8_t RF_BRIGHT25_COMMAND = 0x55;                                     //  25% brightness - MIGHT BE DISMISSED
  const uint8_t RF_SLIDE_RANGE_MIN = 0x80;                                      // Lowest HEX value used by dimm slider
  const uint8_t RF_SLIDE_RANGE_MAX = 0xFF;                                      // Highest HEX value used by dimm slider
  const uint8_t RF_SLIDE_OFFSET = 0xA8;                                         // Offset needed for the slider value conversion
  const uint8_t RF_SLIDE_STEPS = RF_SLIDE_RANGE_MAX - RF_SLIDE_RANGE_MIN + 1;   // steps from 0% to 100% dimm value
  const uint8_t RF_SLIDE_START = RF_SLIDE_RANGE_MIN + RF_SLIDE_OFFSET;          // is the 0% dimm value position

  // Pulse timings and sequence definitions
  const int BASE_PULSE = 200;                                                     // Base pulse length in microseconds
  const int START_SEQUENCE[10] = {1, 1, 1, 1, 1, 1, 1, 1, 6, 6};                  // Start sequence for sync + RF start bit
  const int BIT0_PULSE[2] = {3, 1};                                               // Pulse for bit 0 (HIGH, LOW duration in BASE_PULSE units)
  const int BIT1_PULSE[2] = {1, 3};                                               // Pulse for bit 1 (HIGH, LOW duration in BASE_PULSE units)

  const int SIGNAL_REPETITIONS = 6; // Number of signal repetitions for robustness

  // --- Required ESPHome Methods ---
  /**
   * @brief Called once at ESP startup to initialize pins.
   */
  void setup() override;

  /**
   * @brief Displays the component's configuration in the log.
   */
  void dump_config() override;

  /**
   * @brief Returns the supported features of the light (e.g., on/off, brightness).
   * @return light::LightTraits object with the supported properties.
   */
  light::LightTraits get_traits() override;

  /**
   * @brief Called by Home Assistant to change the state of the light (e.g., brightness).
   * @param state The desired brightness value (0.0 to 1.0).
   */
  void write_state(float state) override;

  // --- Custom methods for Home Assistant Buttons and Logic ---
  /**
   * @brief Sends a specific RF command (e.g., on/off, predefined brightness).
   * @param command The RF command as an 8-bit value.
   */
  void transmit_rf_command(uint8_t command);

  /**
   * @brief Sends a specific RF brightness value (for the slider).
   * @param brightness_value The brightness value in the range of RF_SLIDE_RANGE_MIN to RF_SLIDE_RANGE_MAX.
   */
  void transmit_rf_brightness(uint8_t brightness_value);

 protected:
  GPIOPin *rf_transmit_pin_;     // GPIO Pin for RF transmission
  GPIOPin *led_pin_{nullptr};    // Optional GPIO Pin for a feedback LED (initialized as nullptr)
  uint16_t rf_address_;          // The 2-byte RF address

  // --- Internal helper methods ---
  /**
   * @brief Generates the bit sequence for RF transmission based on a command.
   * @param command The 8-bit command to be integrated into the signal.
   */
  void generate_code_sequence(uint8_t command);

  /**
   * @brief Sends the generated RF signal via the RF transmit pin.
   */
  void send_rf_signal();

  // Internal buffer for the generated code sequence.
  // Max 32 bits * 2 pulses/bit = 64 values (64 high/low changes for 32 bit signal).
  int code_sequence_[64];
  const int CODE_SEQUENCE_SIZE = 64; // Size of the code sequence array
  const int START_SEQUENCE_SIZE = sizeof(START_SEQUENCE) / sizeof(START_SEQUENCE[0]); // Size of the start sequence
};

} // namespace hamulight
} // namespace esphome
