#pragma once

#include <string>

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {
namespace landis {

#define BUF_SIZE 2048

enum class State { Idle, RecieveInit, ReceiveValues };

class LandisSensor : public sensor::Sensor, public PollingComponent, public uart::UARTDevice {
 public:
  void loop() override;
  void update() override;
  void dump_config() override;

  void set_kwh_sensor(sensor::Sensor *s) { kwh_sensor_ = s; }
  void set_volume_sensor(sensor::Sensor *s) { volume_sensor_ = s; }

 protected:
  sensor::Sensor *kwh_sensor_{nullptr};
  sensor::Sensor *volume_sensor_{nullptr};

  State state = State::Idle;

  std::string ReadLine();
  void ParseFirstLine(std::string string);
  std::string ParseDelimiter(std::string stringToParse, std::string first, std::string second);

  void SendRequest();

  bool ReadOnlyFirstLine{true};

  std::string InitMessage{"/LUGCUH50"};
  std::string buffer_string;
};

}  // namespace landis
}  // namespace esphome
