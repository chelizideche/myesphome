#pragma once

#include "esphome/core/log.h"
#include "esphome/core/gpio.h"
#include "esphome/core/component.h"
#include "esphome/components/i2c_soft/softi2c.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace kp18058 {

// KP18058 main component for controlling LED drivers over soft I2C
class KP18058 : public Component {
 public:
  KP18058();
  void setup() override;

  /**
   * Dumps the configuration details, including pin and communication status.
   * Outputs diagnostic information to ESPHome logs.
   */
  void dump_config() override;

  /**
   * Sets the I2C data and clock pins for the soft I2C driver.
   *
   * @param data_pin Pointer to GPIOPin for data line.
   * @param clock_pin Pointer to GPIOPin for clock line.
   */
  void set_i2c_pins(GPIOPin *data_pin, GPIOPin *clock_pin) { i2c_.set_pins(data_pin, clock_pin); }

  /**
   * Assigns an output channel to the KP18058 driver for LED control.
   *
   * @param channel The output channel to assign (1-5).
   * @param output Pointer to the kp18058_output instance for this channel.
   */
  void set_output_channel(uint8_t channel, class KP18058_output *output) { channels_[channel - 1] = output; }

  /**
   * Sets the maximum current for the CW (cold-white) channels.
   *
   * @param max_cw_current Maximum allowed current for CW channels (in mA).
   */
  void set_cw_current(float max_cw_current) { max_cw_current_ = max_cw_current; }

  /**
   * Sets the maximum current for RGB channels.
   *
   * @param max_rgb_current Maximum allowed current for RGB channels (in mA).
   */
  void set_rgb_current(float max_rgb_current) { max_rgb_current_ = max_rgb_current; }

  /**
   * Programs the LED driver by sending I2C commands based on current channel settings.
   * Ensures the driver is configured according to active channel values.
   */
  void program_led_driver();

 protected:
  float max_cw_current_;
  float max_rgb_current_;

 private:
  class KP18058_output *channels_[5];
  class i2c_soft::SoftI2C i2c_;
  bool i2c_ready_;
};

// class represents an output channel for the KP18058 LED driver
class KP18058_output : public output::FloatOutput {
 public:
  /**
   * Constructor for the KP18058_output class.
   * Initializes the channel with default values.
   */
  KP18058_output() : value_(0), parent_(nullptr) {}

  /**
   * Sets the parent KP18058 driver instance for this output channel.
   *
   * @param parent Pointer to the parent KP18058 instance.
   */
  void set_parent(KP18058 *parent) { parent_ = parent; }

  /**
   * Retrieves the current grayscale value for this output channel.
   *
   * @return 10-bit grayscale value (0-1023) representing channel intensity.
   */
  uint16_t get_value() { return this->value_; }

 protected:
  /**
   * Overrides the write_state function to control LED brightness.
   *
   * @param state Float value representing desired brightness (0.0 to 1.0).
   * Converts state to a 10-bit grayscale integer (0-1023) and triggers
   * the parent class to program the LED driver.
   */
  void write_state(float state) override {
    if (state >= 1) {
      state = 1;
    } else if (state <= 0) {
      state = 0;
    }
    // Convert brightness state (0.0 - 1.0) to 10-bit value (0 - 1023).
    value_ = static_cast<uint16_t>(roundf(state * 1023));

    // Request parent to reprogram the LED driver with updated brightness values.
    this->parent_->program_led_driver();
  }

  // 10-bit grayscale value representing intensity (0-1023) of the output.
  uint16_t value_;
  // Pointer to the parent KP18058 driver class for this output channel.
  KP18058 *parent_;
};

}  // namespace kp18058
}  // namespace esphome
