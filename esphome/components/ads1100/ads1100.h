#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"

namespace esphome {
namespace ads1100 {

enum ADS1100Gain {
  ADS1100_GAIN_1 = 0b00,
  ADS1100_GAIN_2 = 0b01,
  ADS1100_GAIN_4 = 0b10,
  ADS1100_GAIN_8 = 0b11,
};

enum ADS1100DataRate {
  ADS1100_DATA_RATE_128_SPS = 0b00,
  ADS1100_DATA_RATE_32_SPS = 0b01,
  ADS1100_DATA_RATE_16_SPS = 0b10,
  ADS1100_DATA_RATE_8_SPS = 0b11,
};

class ADS1100Component : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  /// HARDWARE_LATE setup priority
  float get_setup_priority() const override { return setup_priority::DATA; }

  /// Helper method to request a measurement from the sensor.
  float request_measurement();

  /// Set the gain of the ADC
  void set_gain(ADS1100Gain gain) { gain_ = gain; }
  /// Set the data rate of the ADC
  void set_data_rate(ADS1100DataRate data_rate) { data_rate_ = data_rate; }

 protected:
  uint16_t prev_config_{0};
  ADS1100Gain gain_{ADS1100_GAIN_1};
  ADS1100DataRate data_rate_{ADS1100_DATA_RATE_128_SPS};
};

}  // namespace ads1100
}  // namespace esphome