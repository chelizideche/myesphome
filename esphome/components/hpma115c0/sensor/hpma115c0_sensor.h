#pragma once

#include "esphome/components/sensor/sensor.h"
#include "../hpma115c0.h"

namespace esphome {
namespace hpma115c0 {

class Hpma115C0Sensor : public Hpma115C0Listener, public Component, sensor::Sensor {
 public:
  void dump_config() override;

  void on_new_values(float pm_1_0, float pm_2_5, float pm_4_0, float pm_10_0, float aqi_2_5, float aqi_10_0);
  void set_pm_1_0_sensor(sensor::Sensor *sensor) { pm_1_0_sensor_ = sensor; }
  void set_pm_2_5_sensor(sensor::Sensor *sensor) { pm_2_5_sensor_ = sensor; }
  void set_pm_4_0_sensor(sensor::Sensor *sensor) { pm_4_0_sensor_ = sensor; }
  void set_pm_10_0_sensor(sensor::Sensor *sensor) { pm_10_0_sensor_ = sensor; }
  void set_aqi_2_5_sensor(sensor::Sensor *sensor) { aqi_pm_2_5_sensor_ = sensor; }
  void set_aqi_10_0_sensor(sensor::Sensor *sensor) { aqi_pm_10_0_sensor_ = sensor; }

 protected:
  sensor::Sensor *pm_1_0_sensor_{nullptr};
  sensor::Sensor *pm_2_5_sensor_{nullptr};
  sensor::Sensor *pm_4_0_sensor_{nullptr};
  sensor::Sensor *pm_10_0_sensor_{nullptr};
  sensor::Sensor *aqi_pm_2_5_sensor_{nullptr};
  sensor::Sensor *aqi_pm_10_0_sensor_{nullptr};
};

}  // namespace hpma115c0
}  // namespace esphome
