#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "DFRobot_AirQualitySensor.h"
#include "aqi_calculator_factory.h"

namespace esphome {
namespace dfrobot_air_quality_sensor {

class DFRobotAirQualitySensor : public PollingComponent, public i2c::I2CDevice {
 public:
  DFRobotAirQualitySensor() : PollingComponent(5000) {}

  void set_pm_1_0_sensor(sensor::Sensor *sensor) { pm_1_0_sensor_ = sensor; }
  void set_pm_2_5_sensor(sensor::Sensor *sensor) { pm_2_5_sensor_ = sensor; }
  void set_pm_10_sensor(sensor::Sensor *sensor) { pm_10_sensor_ = sensor; }
  void set_aqi_sensor(sensor::Sensor *sensor) { aqi_sensor_ = sensor; }
  void set_aqi_calculation_type(AQICalculatorType aqi_calc_type) { aqi_calc_type_ = aqi_calc_type; }
  
  void setup() override;
  void update() override;

 protected:
  sensor::Sensor *pm_1_0_sensor_{nullptr};
  sensor::Sensor *pm_2_5_sensor_{nullptr};
  sensor::Sensor *pm_10_sensor_{nullptr};
  sensor::Sensor *aqi_sensor_{nullptr};
  DFRobot_AirQualitySensor *sensor_;
  
  AQICalculatorType aqi_calc_type_;
  AQICalculatorFactory aqi_calculator_factory_ = AQICalculatorFactory();
};

}  // namespace dfrobot_air_quality_sensor
}  // namespace esphome
