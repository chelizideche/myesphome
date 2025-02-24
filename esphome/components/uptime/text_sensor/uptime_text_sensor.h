#pragma once

#include "esphome/core/defines.h"

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace uptime {

class UptimeTextSensor : public text_sensor::TextSensor, public PollingComponent {
 public:
  void update() override;
  void dump_config() override;
  void setup() override;

  void set_separator(const std::string &separator) { this->separator_ = separator; }

  float get_setup_priority() const override;

 protected:
  void insert_buffer_(std::string &buffer, const char *key, unsigned value) const;
  std::string separator_{};
  uint32_t uptime_{0};  // uptime in seconds, will overflow after 136 years
  uint32_t last_ms_{0};
};

}  // namespace uptime
}  // namespace esphome
