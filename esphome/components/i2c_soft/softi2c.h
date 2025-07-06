#pragma once

#include "esphome/core/gpio.h"

namespace esphome {
namespace i2c_soft {

/**
 * @brief Class to implement a software I2C protocol.
 *
 * This class allows communication with I2C devices using GPIO pins
 * to simulate I2C communication through software control.
 */
class SoftI2C {
 public:
  SoftI2C() : data_pin_(nullptr), clock_pin_(nullptr) {}

  /**
   * @brief Sets up the I2C pins.
   *
   * This method initializes the data and clock pin objects by calling
   * their respective setup methods.
   */
  void setup() {
    data_pin_->setup();
    clock_pin_->setup();
  }

  /**
   * @brief Sets the GPIO pins for data and clock.
   *
   * This method associates the provided GPIO pins with the I2C instance.
   *
   * @param data_pin Pointer to the GPIOPin object for the data line (SDA).
   * @param clock_pin Pointer to the GPIOPin object for the clock line (SCL).
   */
  void set_pins(GPIOPin *data_pin, GPIOPin *clock_pin) {
    data_pin_ = data_pin;
    clock_pin_ = clock_pin;
  }

  /**
   * @brief Gets the data pin associated with this I2C instance.
   *
   * @return Pointer to the GPIOPin object for the data line (SDA).
   */
  GPIOPin *get_data_pin() const { return data_pin_; }

  /**
   * @brief Gets the clock pin associated with this I2C instance.
   *
   * @return Pointer to the GPIOPin object for the clock line (SCL).
   */
  GPIOPin *get_clock_pin() const { return clock_pin_; }

  /**
   * @brief Resets the I2C bus and checks for device readiness.
   *
   * This method generates clock pulses to clear any stuck data on the bus,
   * checks the state of the data line (SDA), and sends start-stop signals
   * to ensure proper initialization.
   *
   * @return true if the reset was successful and the bus is free.
   * @return false if the reset failed (SDA remains low).
   */
  bool reset();

  /**
   * @brief Initiates the start condition for I2C communication.
   *
   * This method pulls the data line (SDA) low while the clock line (SCL)
   * is high, signaling the start of communication to the device.
   */
  void start();

  /**
   * @brief Initiates the stop condition for I2C communication.
   *
   * This method releases the data line (SDA) after setting the clock line (SCL) high,
   * signaling the end of communication to the device.
   */
  void stop();

  /**
   * @brief Writes a byte of data to the I2C bus.
   *
   * This method sends a byte by shifting each bit to the data line (SDA).
   * After the byte is sent, it checks for an acknowledgment (ACK) from the device.
   *
   * @param value The byte value to be sent to the I2C bus.
   * @return true if an acknowledgment (ACK) is received from the device.
   * @return false if no acknowledgment (NACK) is received.
   */
  bool write_byte(uint8_t value);

 private:
  /**
   * @brief Sets the specified GPIO pin low.
   *
   * This method configures the pin as an output and writes a low value (false) to it.
   *
   * @param pin Pointer to the GPIOPin object representing the pin to set low.
   */
  void set_low_(GPIOPin *pin) {
    pin->pin_mode(gpio::FLAG_OUTPUT);
    pin->digital_write(false);
  }

  /**
   * @brief Sets the specified GPIO pin high.
   *
   * This method configures the pin as an input with a pull-up resistor.
   *
   * @param pin Pointer to the GPIOPin object representing the pin to set high.
   */
  void set_high_(GPIOPin *pin) { pin->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP); }

  GPIOPin *data_pin_;   ///< Pointer to the GPIOPin object for the data line (SDA).
  GPIOPin *clock_pin_;  ///< Pointer to the GPIOPin object for the clock line (SCL).
};

}  // namespace i2c_soft
}  // namespace esphome
