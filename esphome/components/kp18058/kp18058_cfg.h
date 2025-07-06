#pragma once

namespace esphome {
namespace kp18058 {

/**
 * @brief Enumeration for device working modes.
 */
enum WorkingMode { STANDBY_MODE = 0b00, RGB_MODE = 0b01, CW_MODE = 0b10, RGBCW_MODE = 0b11 };

/**
 * @brief Enumeration for Line Compensation Mode.
 *
 * Controls whether line compensation is enabled or disabled.
 * When the input voltage exceeds the voltage threshold
 * the Line compensation decreases the current linearly with the defined slope.
 */
enum LCMode { LC_DISABLE = 0b0, LC_ENABLE = 0b1 };

/**
 * @brief Enumeration for Line Compensation Start Threshold.
 *
 * Defines the input voltage at which line compensation activates. The slope of compensation
 * changes depending on voltage range and on the Compensation Slope parameter:
 * - For low thresholds (140V to 175V): Compensation slope parameter is defined for 15V increment.
 * - For high thresholds (260V to 330V): Compensation slope parameter is defined for 30V increment.
 */
enum LCThreshold : uint8_t {
  // Low voltage thresholds (15V increments)
  LC_THRESHOLD_140V = 0b0000,
  LC_THRESHOLD_145V = 0b0001,
  LC_THRESHOLD_150V = 0b0010,
  LC_THRESHOLD_155V = 0b0011,
  LC_THRESHOLD_160V = 0b0100,
  LC_THRESHOLD_165V = 0b0101,
  LC_THRESHOLD_170V = 0b0110,
  LC_THRESHOLD_175V = 0b0111,

  // High voltage thresholds (30V increments)
  LC_THRESHOLD_260V = 0b1000,
  LC_THRESHOLD_270V = 0b1001,
  LC_THRESHOLD_280V = 0b1010,
  LC_THRESHOLD_290V = 0b1011,
  LC_THRESHOLD_300V = 0b1100,
  LC_THRESHOLD_310V = 0b1101,
  LC_THRESHOLD_320V = 0b1110,
  LC_THRESHOLD_330V = 0b1111
};

/**
 * @brief Enumeration for Line Compensation Slope Settings.
 *
 * Defines the slope used to decrease the current when line compensation is enabled.
 */
enum LCSlope {
  LC_SLOPE_7_5_PERCENT = 0b00,  /**< current decreases 7.5% */
  LC_SLOPE_10_PERCENT = 0b01,   /**< current decreases 10% */
  LC_SLOPE_12_5_PERCENT = 0b10, /**< current decreases 12.5% */
  LC_SLOPE_15_PERCENT = 0b11    /**< current decreases 15% */
};

/**
 * @brief Enumeration for RC Filter Settings.
 *
 * Controls whether RC filtering is enabled for line compensation calculations.
 * When enabled, it takes an average input voltage; otherwise, it uses the instantaneous input value.
 */
enum RCFilter { RC_FILTER_ENABLE = 0b0, RC_FILTER_DISABLE = 0b1 };

/**
 * @brief Enumeration for Dimming Modes for RGB Channels (1-3).
 *
 * Specifies whether analog dimming or chop dimming is used for RGB channels.
 */
enum DimmingMode {
  ANALOG_DIMMING = 0b0, /**< Analog dimming mode, adjusts LED current amplitude. */
  CHOP_DIMMING = 0b1    /**< Chop dimming mode, adjusts LED current duty cycle. */
};

/**
 * @brief Enumeration for Chop Dimming Frequency Settings.
 *
 * Defines the frequency used for chop dimming mode.
 */
enum CDFrequency : uint8_t {
  CD_FREQUENCY_4KHZ = 0b00,
  CD_FREQUENCY_2KHZ = 0b01,
  CD_FREQUENCY_1KHZ = 0b10,
  CD_FREQUENCY_500HZ = 0b11
};

#pragma pack(push, 1)

/**
 * @brief the structure of the I2C message for configuring the KP18058 LED driver settings.
 */
struct KP18058Settings {
  // Byte 0
  uint8_t byte0_parity_bit : 1;
  uint8_t start_byte_address : 4;
  WorkingMode working_mode : 2;
  uint8_t address_identification : 1;

  // Byte 1
  uint8_t byte1_parity_bit : 1;
  LCSlope line_comp_slope : 2;
  LCThreshold line_comp_threshold : 4;
  LCMode line_compensation_enable : 1;

  // Byte 2
  uint8_t byte2_parity_bit : 1;
  uint8_t max_current_out1_3 : 5;
  CDFrequency chop_dimming_frequency : 2;

  // Byte 3
  uint8_t byte3_parity_bit : 1;
  uint8_t max_current_out4_5 : 5;
  RCFilter rc_filter_enable : 1;
  DimmingMode chop_dimming_out1_3 : 1;

  // Channel array for OUT1 to OUT5 grayscale data
  struct {
    uint8_t upper_parity : 1;
    uint8_t upper_grayscale : 5; /**< upper 5 bits of the channel value */
    uint8_t upper_reserved : 2;

    uint8_t lower_parity : 1;
    uint8_t lower_grayscale : 5; /**< lower 5 bits of the channel value */
    uint8_t lower_reserved : 2;
  } channels[5];
};

#pragma pack(pop)

// Ensures that KP18058Settings size is exactly 14 bytes during compilation
static_assert(sizeof(KP18058Settings) == 14, "Size of KP18058Settings must be exactly 14 bytes");

}  // namespace kp18058
}  // namespace esphome
