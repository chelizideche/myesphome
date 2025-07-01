#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

// datasheet images:
// https://web.archive.org/web/20250202185205/https://ae01.alicdn.com/kf/S12edf1d7f7c64ce7a7e438032c3f9d49d.png
// https://web.archive.org/web/20240929213700/https://ae01.alicdn.com/kf/S293cc7fc3c21428cb08d9cec0b18a2210.png
// https://web.archive.org/web/20240929213700/https://ae01.alicdn.com/kf/S9c261502e9264a638a12ff80e7da7739h.png
// https://web.archive.org/web/20240929213700/https://ae01.alicdn.com/kf/S6b4bae0b6f2c4257a10a230ca93cd23fu.png

namespace esphome {
namespace dc01 {

class DC01Component : public Component, public uart::UARTDevice {
 public:
  DC01Component() = default;

  void set_pm_2_5_sensor(sensor::Sensor *pm_2_5_sensor) { this->pm_2_5_sensor_ = pm_2_5_sensor; }
  void setup() override;
  void dump_config() override;
  void loop() override;

  float get_setup_priority() const override;

 protected:
  optional<bool> check_byte_() const;
  void parse_data_();
  uint8_t dc01_checksum_(const uint8_t *command_data) const;

  sensor::Sensor *pm_2_5_sensor_{nullptr};

  uint8_t data_[20];
  uint8_t data_index_{0};
  uint32_t last_transmission_{0};
};

}  // namespace dc01
}  // namespace esphome
