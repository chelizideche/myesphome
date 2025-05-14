#include "dallas_temp_searcher.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"
#include <cstring>

namespace esphome {
namespace dallas_temp_searcher {

static const char *const TAG = "dallas.temp.searcher";

void DallasTemperatureSearcher::setup() {
  if (this->bus_ == nullptr)
    return;

  const std::vector<uint64_t> &addresses = this->bus_->get_devices();

  this->sensors_params_.reserve(addresses.size());
  this->sensors_.reserve(addresses.size());

  const size_t string_buffer_size = 32;
  char string_buff[string_buffer_size];

  for (const uint64_t &address : addresses) {
    auto *sensor = new dallas_temp::DallasTemperatureSensor();
    EntityBaseInfo entity_base_info;
    sensor->set_one_wire_bus(bus_);

    std::string address_str = format_hex(address);

    strcpy(string_buff, "Temp Sensor 0x");
    strlcat(string_buff, address_str.c_str(), string_buffer_size);
    entity_base_info.name = string_buff;

    strcpy(string_buff, "ts_0x");
    strlcat(string_buff, address_str.c_str(), string_buffer_size);
    entity_base_info.object_id = string_buff;

    sensor->set_name(entity_base_info.name.c_str());
    sensor->set_object_id(entity_base_info.object_id.c_str());
    sensor->set_address(address);
    set_default_parameters_(sensor);

    this->sensors_.push_back(sensor);
    this->sensors_params_.push_back(std::move(entity_base_info));

    App.register_sensor(sensor);
    App.register_component(sensor);
  }
}

void DallasTemperatureSearcher::dump_config() {
  ESP_LOGCONFIG(TAG, "Dallas sensor searcher:");
  for (dallas_temp::DallasTemperatureSensor *sensor : this->sensors_) {
    ESP_LOGCONFIG(TAG, "  Added %s", sensor->get_name().c_str());
  }
}

void DallasTemperatureSearcher::set_default_parameters_(dallas_temp::DallasTemperatureSensor *sensor) {
  sensor->set_device_class("temperature");
  sensor->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
  sensor->set_unit_of_measurement("\302\260C");
  sensor->set_accuracy_decimals(1);
  sensor->set_force_update(false);
  sensor->set_update_interval(this->update_interval_ms_);
  sensor->set_component_source("dallas_temp.sensor");
  sensor->set_resolution(12);
}

}  // namespace dallas_temp_searcher
}  // namespace esphome
