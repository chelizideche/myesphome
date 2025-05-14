#pragma once

#include "esphome/core/component.h"
#include "esphome/components/dallas_temp/dallas_temp.h"
#include <vector>

namespace esphome {
namespace dallas_temp_searcher {

// Class for dynamic creation DallasTemperatureSensor for sensors connected to onewire bus
class DallasTemperatureSearcher : public Component, public one_wire::OneWireDevice {
 public:
  void setup() override;

  // setup should be called before setup dallas temp sensors
  float get_setup_priority() const override { return setup_priority::DATA + 1; }

  // Update interval for all sensors
  void set_update_interval(uint32_t update_interval_ms) { this->update_interval_ms_ = update_interval_ms; }

  void dump_config() override;

  uint16_t sensors_size() { return this->sensors_.size(); }

  dallas_temp::DallasTemperatureSensor *sensor(uint16_t index) {
    if (index >= this->sensors_.size())
      return nullptr;
    return this->sensors_[index];
  }

 protected:
  void set_default_parameters_(dallas_temp::DallasTemperatureSensor *sensor);

  std::vector<dallas_temp::DallasTemperatureSensor *> sensors_;
  std::vector<EntityBaseInfo> sensors_params_;
  uint32_t update_interval_ms_ = 60000;
};

}  // namespace dallas_temp_searcher
}  // namespace esphome
