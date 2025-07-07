#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace wts01 {

class WTS01Sensor : public sensor::Sensor, public uart::UARTDevice, public Component {
 public:
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  float get_temperature() const { return this->current_temperature_; }

 protected:
  static const uint8_t PACKET_SIZE = 9;
  static const uint8_t HEADER_1 = 0x55;
  static const uint8_t HEADER_2 = 0x01;
  static const uint8_t HEADER_3 = 0x01;
  static const uint8_t HEADER_4 = 0x04;

  uint8_t buffer_[PACKET_SIZE];
  uint8_t buffer_pos_{0};
  float current_temperature_{NAN};

  void handle_char_(uint8_t c);
  void process_packet_();

 private:
  static constexpr const char *TAG = "wts01";
};

}  // namespace wts01
}  // namespace esphome
