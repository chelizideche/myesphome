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

enum ADS1100SampleRate {
  ADS1100_SAMPLE_RATE_128_SPS = 0b00,
  ADS1100_SAMPLE_RATE_32_SPS = 0b01,
  ADS1100_SAMPLE_RATE_16_SPS = 0b10,
  ADS1100_SAMPLE_RATE_8_SPS = 0b11,
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
  /// Get the current gain setting
  ADS1100Gain get_gain() const { return gain_; }

  /// Set the sample rate of the ADC
  void set_sample_rate(ADS1100SampleRate sample_rate) { sample_rate_ = sample_rate; }
  /// Get the current sample rate setting
  ADS1100SampleRate get_sample_rate() const { return sample_rate_; }

 protected:
  uint16_t prev_config_{0};
  ADS1100Gain gain_{ADS1100_GAIN_1};
  ADS1100SampleRate sample_rate_{ADS1100_SAMPLE_RATE_128_SPS};
  bool i2c_initialized_{false};
};

}  // namespace ads1100
}  // namespace esphome