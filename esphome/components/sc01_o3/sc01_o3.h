#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/defines.h"
#include "esphome/core/component.h"

namespace esphome {
namespace sc01_o3 {

class SC01O3Sensor : public sensor::Sensor, public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;

  uint16_t state = 0;

 protected:
  bool is_initialized_ = false;
  void set_qna_mode_();
  static uint8_t calculate_checksum(const uint8_t *data, uint8_t len);
  void send_command_(std::array<uint8_t, 7> data);
};

}  // namespace sc01_o3
}  // namespace esphome
