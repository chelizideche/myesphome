
#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {
namespace ld2413 {

class LD2413Sensor : public sensor::Sensor, public uart::UARTDevice, public PollingComponent {
 public:
  void setup() override;
  void loop() override;
  void update() override;

 protected:
  uint32_t last_publish_{0};
  float latest_distance_cm_{NAN};
};

}  // namespace ld2413
}  // namespace esphome
