#pragma once

#include "esphome.h"

extern "C" {
#include "VL53L4CD_api.h"
#include "VL53L4CD_calibration.h"
#include "platform.h"
}

namespace esphome {
namespace vl53l4cd {

class VL53L4CDSensor : public PollingComponent, public sensor::Sensor, public i2c::I2CDevice {
 public:
  VL53L4CDSensor() : PollingComponent(1000) {}

  void setup() override {
    ESP_LOGI("VL53L4CD", "Initializing...");
    Wire.begin();

    dev_.I2cDevAddr = 0x29;
    dev_.comms_type = 1;
    dev_.comms_speed_khz = 400;
    auto dev = this->address_;  // ← 获取 YAML 中的 I2C 地址
   	uint16_t 				sensor_id;


    delay(1000);

    int status;

    status = VL53L4CD_GetSensorId(dev_.I2cDevAddr, &sensor_id);
    if(status || (sensor_id != 0xEBAA))
    {
      ESP_LOGE("VL53L4CD", "VL53L4CD not detected at requested address");
      this->mark_failed();
      return;
    }

    status = VL53L4CD_SensorInit(dev_.I2cDevAddr);
    if (status != 0) {
      ESP_LOGE("VL53L4CD", "SensorInit failed with status %d", status);
      this->mark_failed();
      return;
    }

    if (VL53L4CD_SetRangeTiming(dev_.I2cDevAddr, 50, 0) != 0) {
      ESP_LOGW("VL53L4CD", "SetRangeTiming failed");
    }

    if (VL53L4CD_StartRanging(dev_.I2cDevAddr) != 0) {
      ESP_LOGE("VL53L4CD", "StartRanging failed");
      this->mark_failed();
      return;
    }


    ESP_LOGI("VL53L4CD", "VL53L4CD ready");
  }

  void update() override {
    uint8_t isReady = 0;
    VL53L4CD_ResultsData_t result;
    int status = VL53L4CD_CheckForDataReady(dev_.I2cDevAddr, &isReady);
    if (status != 0 || !isReady) {
      ESP_LOGW("VL53L4CD", "Data not ready or check failed");
      return;
    }

    /* (Mandatory) Clear HW interrupt to restart measurements */
    VL53L4CD_ClearInterrupt(dev_.I2cDevAddr);

    status = VL53L4CD_GetResult(dev_.I2cDevAddr, &result);
    if (status == 0) {
      ESP_LOGD("VL53L4CD", "Distance: %d mm", result.distance_mm);
      publish_state(result.distance_mm);
    } else {
      ESP_LOGW("VL53L4CD", "Failed to get result");
    }
  }

 protected:
  VL53L4CD_Dev_t dev_;
};

}  // namespace vl53l4cd
}  // namespace esphome
