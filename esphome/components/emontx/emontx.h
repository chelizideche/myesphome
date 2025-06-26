#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
// Conditionally include sensor
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
// Conditionally include http_request
#ifdef USE_HTTP_REQUEST
#include "esphome/components/http_request/http_request.h"
#endif

#ifdef USE_MQTT_FORWARD
#include "esphome/components/mqtt/mqtt_client.h"
#endif

#include <map>
#include <vector>

namespace esphome {
namespace emontx {
/*
 * 198 bytes should be enough to contain a full session in historical mode with
 * three phases. But go with 1024 just to be sure.
 */
/**
 * @class EmonTxListener
 * @brief Listener interface for receiving updates from the EmonTx.
 *
 * This class allows other components to register as listeners to receive updates
 * for specific tags published by the EmonTx.
 */
class EmonTxListener {
 public:
  std::string tag;
  virtual void publish_val(const std::string &val){};
};

// Define the enum for publish modes
enum class MqttPublishMode { JSON, INDIVIDUAL };

/**
 * @class EmonTx
 * @brief Main class for the EmonTx component.
 *
 * The EmonTx processes incoming data frames via UART, validates their CRC,
 * extracts tags and values, and publishes them to registered listeners.
 */
class EmonTx : public PollingComponent, public uart::UARTDevice {
 public:
  EmonTx() = default;

  void register_emontx_listener(EmonTxListener *listener);
  void loop() override;
  void setup() override;
  void update() override;
  void dump_config() override;
  std::vector<EmonTxListener *> emontx_listeners_{};

#ifdef USE_SENSOR
  void register_sensor(const std::string &tag_name, sensor::Sensor *sensor);
#endif

#ifdef USE_HTTP_REQUEST
  // EmonCMS configuration - only available when http_request is enabled
  void set_http_forward(const std::string &server, const std::string &node, const std::string &apikey,
                        http_request::HttpRequestComponent *http_client) {
    emoncms_server_ = server;
    emoncms_node_ = node;
    emoncms_apikey_ = apikey;
    http_client_ = http_client;
    has_emoncms_config_ = true;
  }
#endif

#ifdef USE_MQTT_FORWARD
  // MQTT forwarding configuration
  void set_mqtt_config(const std::string &base_prefix, const std::string &topic_prefix,
                       const std::string &publish_mode = "json") {
    mqtt_base_prefix_ = base_prefix;
    mqtt_topic_prefix_ = topic_prefix;

    // Convert string to enum
    if (publish_mode == "individual") {
      mqtt_publish_mode_ = MqttPublishMode::INDIVIDUAL;
    } else {
      // Default to JSON
      mqtt_publish_mode_ = MqttPublishMode::JSON;
    }

    has_mqtt_config_ = true;

    // Pre-compute the full topic prefix
    mqtt_topic_prefix_full_ = mqtt_base_prefix_;
    if (!mqtt_topic_prefix_full_.empty() && mqtt_topic_prefix_full_.back() != '/') {
      mqtt_topic_prefix_full_ += '/';
    }
  }
#endif

 protected:
#ifdef USE_SENSOR
  std::map<std::string, sensor::Sensor *> sensors_{};
#endif
  std::string buffer_;
  void parse_json_(const std::string &data);
  void publish_value_(const std::string &tag, const std::string &val);

#ifdef USE_HTTP_REQUEST
  // EmonCMS configuration - only declared when http_request is enabled
  bool has_emoncms_config_{false};
  std::string emoncms_server_;
  std::string emoncms_node_;
  std::string emoncms_apikey_;
  http_request::HttpRequestComponent *http_client_{nullptr};

  // Pre-built URL and body template
  std::string emoncms_url_;          // The base URL including endpoint
  std::string emoncms_body_prefix_;  // "node=X&apikey=Y&json="

  void send_to_emoncms_(const std::string &json_data);
#endif

#ifdef USE_MQTT_FORWARD
  // MQTT forwarding config
  bool has_mqtt_config_{false};
  std::string mqtt_base_prefix_;  // Default base prefix
  std::string mqtt_topic_prefix_;
  // Pre-computed MQTT topic prefix (base_prefix/topic_prefix/)
  std::string mqtt_topic_prefix_full_;
  MqttPublishMode mqtt_publish_mode_{MqttPublishMode::JSON};  // Use enum now

  void send_to_mqtt_(const std::string &json_data);
#endif
};

}  // namespace emontx
}  // namespace esphome
