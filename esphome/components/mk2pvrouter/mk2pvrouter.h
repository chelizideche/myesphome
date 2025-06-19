#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

#ifdef USE_MQTT_FORWARD
#include "esphome/components/mqtt/mqtt_client.h"
#endif

#include <vector>
#include <array>

namespace esphome {
namespace mk2pvrouter {
/*
 * 198 bytes should be enough to contain a full session in historical mode with
 * three phases. But go with 1024 just to be sure.
 */
constexpr uint8_t MAX_TAG_SIZE = 16;
constexpr uint16_t MAX_VAL_SIZE = 16;
constexpr uint16_t MAX_BUF_SIZE = 1048;

/**
 * @class Mk2PVRouterListener
 * @brief Listener interface for receiving updates from the Mk2PVRouter.
 *
 * This class allows other components to register as listeners to receive updates
 * for specific tags published by the Mk2PVRouter.
 */
class Mk2PVRouterListener {
 public:
  std::string tag;
  virtual void publish_val(const std::string &val){};
};

/**
 * @class Mk2PVRouter
 * @brief Main class for the Mk2PVRouter component.
 *
 * The Mk2PVRouter processes incoming data frames via UART, validates their CRC,
 * extracts tags and values, and publishes them to registered listeners.
 */
class Mk2PVRouter : public PollingComponent, public uart::UARTDevice {
 public:
  Mk2PVRouter();
  void register_mk2pvrouter_listener(Mk2PVRouterListener *listener);
  void loop() override;
  void setup() override;
  void update() override;
  void dump_config() override;
  std::vector<Mk2PVRouterListener *> mk2pvrouter_listeners_{};

#ifdef USE_MQTT_FORWARD
  // MQTT forwarding configuration
  void set_mqtt_forward(const std::string &topic_prefix) {
    mqtt_topic_prefix_ = topic_prefix;
    has_mqtt_config_ = true;
  }
#endif

 protected:
  uint32_t baud_rate_;
  size_t checksum_area_end_;
  std::array<char, MAX_BUF_SIZE> buf_;
  size_t buf_index_{0};
  std::string tag_;
  std::string val_;

  enum class State {
    OFF,
    ON,
    START_FRAME_RECEIVED,
    END_FRAME_RECEIVED,
  };

  State state_{State::OFF};

  bool read_chars_until_(bool drop, uint8_t c);
  uint8_t calculate_crc_(const char *grp, size_t grp_len);
  bool check_crc_(const char *grp, const char *grp_end);
  void publish_value_(const std::string &tag, const std::string &val);

#ifdef USE_MQTT_FORWARD
  // MQTT forwarding config
  bool has_mqtt_config_{false};
  std::string mqtt_topic_prefix_;
  void send_to_mqtt_(const std::string &tag, const std::string &value);
#endif
};
}  // namespace mk2pvrouter
}  // namespace esphome
