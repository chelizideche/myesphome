#include "hpma115c0_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hpma115c0 {

static const char *const TAG = "hpma115c0.sensor";

void Hpma115C0Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "HPMA115C0 Sensor:");
  LOG_SENSOR("  ", "pm 1.0µm", this->pm_1_0_sensor_);
  LOG_SENSOR("  ", "pm 2.5µm", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "pm 4.0µm", this->pm_4_0_sensor_);
  LOG_SENSOR("  ", "pm 10.0µm", this->pm_10_0_sensor_);
  LOG_SENSOR("  ", "aqi 2.5", this->aqi_pm_2_5_sensor_);
  LOG_SENSOR("  ", "aqi 10", this->aqi_pm_2_5_sensor_);
}

void Hpma115C0Sensor::on_new_values(float pm_1_0, float pm_2_5, float pm_4_0, float pm_10_0, float aqi_2_5,
                                    float aqi_10_0) {
  if (this->pm_1_0_sensor_ != nullptr) {
    this->pm_1_0_sensor_->publish_state(pm_1_0);
  }
  if (this->pm_2_5_sensor_ != nullptr) {
    this->pm_2_5_sensor_->publish_state(pm_2_5);
  }
  if (this->pm_4_0_sensor_ != nullptr) {
    this->pm_4_0_sensor_->publish_state(pm_4_0);
  }
  if (this->pm_10_0_sensor_ != nullptr) {
    this->pm_10_0_sensor_->publish_state(pm_10_0);
  }
  if (this->aqi_pm_2_5_sensor_ != nullptr) {
    this->aqi_pm_2_5_sensor_->publish_state(aqi_2_5);
  }
  if (this->aqi_pm_10_0_sensor_ != nullptr) {
    this->aqi_pm_10_0_sensor_->publish_state(aqi_10_0);
  }
}

}  // namespace hpma115c0
}  // namespace esphome
