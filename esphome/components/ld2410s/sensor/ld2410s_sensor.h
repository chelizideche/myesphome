#pragma once

#include "../ld2410s.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace ld2410s {
class LD2410SSensor : public LD2410SListener, public Component, sensor::Sensor {
 public:
  void set_distance_sensor(sensor::Sensor *sensor) { this->distance_sensor_ = sensor; }
  void set_threshold_update_sensor(sensor::Sensor *sensor) { this->threshold_update_sensor_ = sensor; }
  void on_distance(uint16_t distance) override {
    if (this->distance_sensor_ != nullptr) {
      if (this->distance_sensor_->get_state() != distance) {
        this->distance_sensor_->publish_state(distance);
      }
    }
  }
  void on_threshold_progress(uint16_t progress) override {
    if (this->threshold_update_sensor_ != nullptr) {
      if (this->threshold_update_sensor_->get_state() != progress) {
        this->threshold_update_sensor_->publish_state(progress);
      }
    }
  };

 private:
  sensor::Sensor *distance_sensor_{nullptr};
  sensor::Sensor *threshold_update_sensor_{nullptr};
};
}  // namespace ld2410s
}  // namespace esphome
