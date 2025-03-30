#pragma once

#include "../LD2410S.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace ld2410s {
class LD2410STextSensor : public LD2410SListener, public Component, text_sensor::TextSensor {
 public:
  void set_fw_version_text_sensor(text_sensor::TextSensor *tsensor) { this->fw_version_text_sensor = tsensor; };
  void on_fw_version(std::string &val) override {
    if (this->fw_version_text_sensor != nullptr) {
      this->fw_version_text_sensor->publish_state(val);
    }
  }

  void set_trigger_threshold_text_sensor(text_sensor::TextSensor *tsensor) {
    this->trigger_threshold_text_sensor = tsensor;
  };
  void on_trigger_threshold_ts(std::string &val) override {
    if (this->trigger_threshold_text_sensor != nullptr) {
      this->trigger_threshold_text_sensor->publish_state(val);
    }
  }

  void set_trigger_hold_text_sensor(text_sensor::TextSensor *tsensor) { this->trigger_hold_text_sensor = tsensor; };
  void on_trigger_hold_ts(std::string &val) override {
    if (this->trigger_hold_text_sensor != nullptr) {
      this->trigger_hold_text_sensor->publish_state(val);
    }
  }

  void set_trigger_snr_text_sensor(text_sensor::TextSensor *tsensor) { this->trigger_snr_text_sensor = tsensor; };
  void on_trigger_snr_ts(std::string &val) override {
    if (this->trigger_snr_text_sensor != nullptr) {
      this->trigger_snr_text_sensor->publish_state(val);
    }
  }

  void set_energy_values_text_sensor(text_sensor::TextSensor *tsensor) { this->energy_values_text_sensor = tsensor; };
  void on_energy_values_ts(std::string &val) override {
    if (this->energy_values_text_sensor != nullptr) {
      this->energy_values_text_sensor->publish_state(val);
    }
  }

 private:
  text_sensor::TextSensor *fw_version_text_sensor{nullptr};
  text_sensor::TextSensor *trigger_threshold_text_sensor{nullptr};
  text_sensor::TextSensor *trigger_hold_text_sensor{nullptr};
  text_sensor::TextSensor *trigger_snr_text_sensor{nullptr};
  text_sensor::TextSensor *energy_values_text_sensor{nullptr};
};

}  // namespace ld2410s
}  // namespace esphome