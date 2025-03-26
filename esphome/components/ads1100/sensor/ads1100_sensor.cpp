#include "ads1100_sensor.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ads1100 {

static const char *const TAG = "ads1100.sensor";

void ADS1100Sensor::setup() { ESP_LOGCONFIG(TAG, "Setting up ADS1100 Sensor '%s'...", this->name_.c_str()); }

void ADS1100Sensor::dump_config() {
  LOG_SENSOR("", "ADS1100 Sensor", this);
  LOG_UPDATE_INTERVAL(this);
}

void ADS1100Sensor::update() {
  float value = this->parent_->request_measurement();
  if (std::isnan(value)) {
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();
  this->publish_state(value);
}

}  // namespace ads1100
}  // namespace esphome