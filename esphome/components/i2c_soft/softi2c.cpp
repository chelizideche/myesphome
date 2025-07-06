#include "softi2c.h"

namespace esphome {
namespace i2c_soft {

// Hold time is 250 ns
static const uint8_t SOFT_I2C_CLOCK_TIME = 50;

void ns_sleep(int ns_delay) {
  // Create a delay by executing NOP instructions in a loop.
  for (volatile int i = 0; i < ns_delay; i += 1)
    __asm__("nop");
}

bool SoftI2C::reset() {
  // Ensure SDA is released (high) to avoid conflict during reset
  set_high_(data_pin_);

  // Clock SCL up to 9 times to clear any stuck data
  for (int i = 0; i < 9; ++i) {
    set_high_(clock_pin_);
    ns_sleep(SOFT_I2C_CLOCK_TIME / 2);
    set_low_(clock_pin_);
    ns_sleep(SOFT_I2C_CLOCK_TIME / 2);

    // Check if SDA is released (high) by the device during clocking
    if (data_pin_->digital_read()) {
      break;  // SDA released, bus should be free now
    }
  }

  // Final check: If SDA is still low, reset failed
  if (!data_pin_->digital_read()) {
    return false;  // SDA stuck low, reset unsuccessful
  }

  // send a start-stop signals during intialisation
  // in order to prevent the chip from misjudging
  // the Start signal and causing data errors
  start();
  stop();

  // Final verification: both SDA and SCL should be high (bus idle)
  return (data_pin_->digital_read()) && (clock_pin_->digital_read());
}

bool SoftI2C::write_byte(uint8_t value) {
  ns_sleep(SOFT_I2C_CLOCK_TIME / 2);

  for (uint8_t curr = 0x80; curr != 0; curr >>= 1) {
    if (curr & value) {
      set_high_(data_pin_);
    } else {
      set_low_(data_pin_);
    }
    set_high_(clock_pin_);
    ns_sleep(SOFT_I2C_CLOCK_TIME);
    set_low_(clock_pin_);
    // Data is written to the register on the falling edge of SCL
    // it needs to be valid through at least HOLD TIME
    // waiting half a cycle assuming it is longer than HOLD_TIME
    ns_sleep(SOFT_I2C_CLOCK_TIME / 2);
  }

  // Every time the transmission of 8bit data (one byte)
  // is completed, in the ninth SCL, KP18058 internally
  // generates a response signal ACK
  bool ack_received;
  set_high_(data_pin_);
  set_high_(clock_pin_);
  ns_sleep(SOFT_I2C_CLOCK_TIME / 2);
  ack_received = !data_pin_->digital_read();
  ns_sleep(SOFT_I2C_CLOCK_TIME / 2);
  set_low_(clock_pin_);

  return ack_received;
}

void SoftI2C::start() {
  set_low_(data_pin_);
  // It needs to be valid through at least HOLD TIME
  // Waiting half a cycle. Assuming it is longer than HOLD_TIME
  ns_sleep(SOFT_I2C_CLOCK_TIME / 2);
  set_low_(clock_pin_);
}

void SoftI2C::stop() {
  ns_sleep(SOFT_I2C_CLOCK_TIME / 2);
  set_low_(data_pin_);
  // It needs to be valid through at least HOLD TIME
  // Waiting half a cycle. Assuming it is longer than HOLD_TIME
  ns_sleep(SOFT_I2C_CLOCK_TIME / 2);
  set_high_(clock_pin_);
  ns_sleep(SOFT_I2C_CLOCK_TIME / 2);
  set_high_(data_pin_);
}

}  // namespace i2c_soft
}  // namespace esphome
