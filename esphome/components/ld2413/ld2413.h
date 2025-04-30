#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {
namespace ld2413 {

class LD2413Sensor : public sensor::Sensor, public uart::UARTDevice, public PollingComponent {
 public:
  void setup() override;
  void update() override;

 protected:
  void read_sensor_();
};

}  // namespace ld2413
}  // namespace esphome
