#pragma once
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include <vector>

namespace esphome {
namespace sm300d2 {

class SM300D2Sensor : public PollingComponent, public uart::UARTDevice {
public:
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_addr_sensor(sensor::Sensor *addr_sensor) { addr_sensor_ = addr_sensor; }
  void set_function_sensor(sensor::Sensor *function_sensor) { function_sensor_ = function_sensor; }
  void set_co2_sensor(sensor::Sensor *co2_sensor) { co2_sensor_ = co2_sensor; }
  void set_formaldehyde_sensor(sensor::Sensor *formaldehyde_sensor) {
    formaldehyde_sensor_ = formaldehyde_sensor;
  }
  void set_tvoc_sensor(sensor::Sensor *tvoc_sensor) { tvoc_sensor_ = tvoc_sensor; }
  void set_pm_2_5_sensor(sensor::Sensor *pm_2_5_sensor) { pm_2_5_sensor_ = pm_2_5_sensor; }
  void set_pm_10_0_sensor(sensor::Sensor *pm_10_0_sensor) { pm_10_0_sensor_ = pm_10_0_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) {
    temperature_sensor_ = temperature_sensor;
  }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }

  void update() override;
  void dump_config() override;

protected:
  uint16_t sm300d2_checksum_(const uint8_t *ptr, uint8_t length);
  void process_packet(const std::vector<uint8_t> &packet, bool is_new_revision);

  sensor::Sensor *addr_sensor_{nullptr};
  sensor::Sensor *function_sensor_{nullptr};
  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *formaldehyde_sensor_{nullptr};
  sensor::Sensor *tvoc_sensor_{nullptr};
  sensor::Sensor *pm_2_5_sensor_{nullptr};
  sensor::Sensor *pm_10_0_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
};

}
}
