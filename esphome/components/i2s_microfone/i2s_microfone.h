#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/core/gpio.h"

namespace esphome {
namespace i2s_microfone {

class I2SMicrofoneSensor : public sensor::Sensor, public PollingComponent {
 public:
  void set_pins(InternalGPIOPin *bck, InternalGPIOPin *ws, InternalGPIOPin *data_in) {
    this->bck_pin_ = bck;
    this->ws_pin_ = ws;
    this->data_in_pin_ = data_in;
  }

  void set_calibration_offset(float offset) { this->calibration_offset_ = offset; }
  void set_calibration_number(number::Number *number) { this->calibration_number_ = number; }
  
  void setup() override;
  void update() override;
  
 protected:
  InternalGPIOPin *bck_pin_;
  InternalGPIOPin *ws_pin_;
  InternalGPIOPin *data_in_pin_;
  float calibration_offset_{0.0f}; 
  number::Number *calibration_number_{nullptr};
};

}
}
