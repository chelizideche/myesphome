#include "dfrobot_air_quality_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dfrobot_air_quality_sensor {

static const char *const TAG = "dfrobot_air_quality";

void DFRobotAirQualitySensor::setup() {
  ESP_LOGI(TAG, "Starting DFRobot Air Quality Sensor...");

  sensor_ = new DFRobot_AirQualitySensor(&Wire, this->address_);

  if (!sensor_->begin()) {
    ESP_LOGE(TAG, "Error initializing sensor!");
    mark_failed();
    return;
  }

  ESP_LOGI(TAG, "Sensor initialized successfully.");
  
}

void DFRobotAirQualitySensor::update() {
  uint16_t pm1  = sensor_->gainParticleConcentration_ugm3(PARTICLE_PM1_0_STANDARD);
  uint16_t pm25 = sensor_->gainParticleConcentration_ugm3(PARTICLE_PM2_5_STANDARD);
  uint16_t pm10 = sensor_->gainParticleConcentration_ugm3(PARTICLE_PM10_STANDARD);
  
  uint16_t pm03 = sensor_->gainParticleConcentration_ugm3(PARTICLENUM_0_3_UM_EVERY0_1L_AIR);
  uint16_t pm05 = sensor_->gainParticleConcentration_ugm3(PARTICLENUM_0_5_UM_EVERY0_1L_AIR);
  uint16_t pm50 = sensor_->gainParticleConcentration_ugm3(PARTICLENUM_5_0_UM_EVERY0_1L_AIR);
  uint16_t pm105= sensor_->gainParticleConcentration_ugm3(PARTICLENUM_10_UM_EVERY0_1L_AIR);
  
  int16_t aqi = -1;
  if (pm25 != -1 && pm10 != -1) {
    AbstractAQICalculator *calculator = aqi_calculator_factory_.get_calculator(aqi_calc_type_);
    aqi = calculator->get_aqi(pm25, pm10);
  }

  ESP_LOGD(TAG, "PM1.0: %u µg/m³, PM2.5: %u µg/m³, PM10: %u µg/m³", pm1, pm25, pm10);

  if (pm_1_0_sensor_ != nullptr)
    pm_1_0_sensor_->publish_state(pm1);
    
  if (pm_2_5_sensor_ != nullptr)
    pm_2_5_sensor_->publish_state(pm25);
    
  if (pm_10_sensor_ != nullptr)
    pm_10_sensor_->publish_state(pm10);
    
  if (aqi_sensor_ != nullptr)
    aqi_sensor_->publish_state(aqi);    
    
  if (pm_0_3_sensor_ != nullptr)
    pm_0_3_sensor_->publish_state(pm03);
    
  if (pm_0_5_sensor_ != nullptr)
    pm_0_5_sensor_->publish_state(pm05);
    
  if (pm_5_0_sensor_ != nullptr)
    pm_5_0_sensor_->publish_state(pm50);
    
  if (pm_10_5_sensor_ != nullptr)
  pm_10_5_sensor_->publish_state(pm105);

}

}  // namespace dfrobot_air_quality_sensor
}  // namespace esphome
