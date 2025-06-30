 #pragma once
 
 #include <cstdint>
 
 namespace esphome {
 namespace dfrobot_air_quality_sensor {
 
 class AbstractAQICalculator {
  public:
   virtual uint16_t get_aqi(uint16_t pm2_5_value, uint16_t pm10_0_value) = 0;
 };
 
 }  // namespace dfrobot_air_quality_sensor
 }  // namespace esphome