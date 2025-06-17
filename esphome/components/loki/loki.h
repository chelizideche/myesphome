#pragma once
#include "esphome/core/defines.h"
#ifdef USE_NETWORK

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/log.h"

namespace esphome {
namespace loki {

class LokiComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  /// Loki client setup priority
  float get_setup_priority() const override;
  /// Logging specific
  void set_log_level(int level);
  /// Get the topic used for logging. Defaults to "<topic_prefix>/debug" and the value is cached for speed.
  void disable_log_message();
  bool is_log_message_enabled() const;
  std::string log_message_;
  std::string payload_buffer_;
  int log_level_{ESPHOME_LOG_LEVEL};
};

}  // namespace loki
}  // namespace esphome
#endif
