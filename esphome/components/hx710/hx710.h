#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"

#include <cinttypes>

namespace esphome {
namespace hx710 {

enum HX710Mode : uint8_t {
  HX710_DIFFERENTIAL_INPUT_10HZ = 1,
  HX710_OTHER_INPUT_40HZ = 2,
  HX710_DIFFERENTIAL_INPUT_40HZ = 3
};

class HX710Sensor : public sensor::Sensor, public PollingComponent, public voltage_sampler::VoltageSampler {
 public:
  void set_dout_pin(GPIOPin *dout_pin) { this->dout_pin_ = dout_pin; }
  void set_sck_pin(GPIOPin *sck_pin) { this->sck_pin_ = sck_pin; }
  void set_gain(HX710Mode gain) { this->gain_ = gain; }
  void set_reference_voltage(float reference_voltage) { this->reference_voltage_ = reference_voltage; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;
  float sample() override;

 protected:
  bool read_sensor_(int32_t *result);
  float reference_voltage_{0.0f};
  GPIOPin *dout_pin_;
  GPIOPin *sck_pin_;
  HX710Mode gain_{HX710_DIFFERENTIAL_INPUT_10HZ};
};

}  // namespace hx710
}  // namespace esphome
