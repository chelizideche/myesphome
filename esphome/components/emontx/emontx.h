#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/http_request/http_request.h"

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

  void register_sensor(const std::string &tag_name, sensor::Sensor *sensor);

  // Set EmonCMS configuration
  void set_emoncms_config(const std::string &server, const std::string &node, const std::string &apikey,
                          http_request::HttpRequestComponent *http_client) {
    emoncms_server_ = server;
    emoncms_node_ = node;
    emoncms_apikey_ = apikey;
    http_client_ = http_client;
    has_emoncms_config_ = true;
  }

 protected:
  std::map<std::string, sensor::Sensor *> sensors_{};
  std::string buffer_;
  void parse_json_(const std::string &data);

  // EmonCMS configuration
  bool has_emoncms_config_{false};
  std::string emoncms_server_;
  std::string emoncms_node_;
  std::string emoncms_apikey_;

  // Reference to external HTTP client component
  http_request::HttpRequestComponent *http_client_{nullptr};

  // Send data to EmonCMS
  void send_to_emoncms_(const std::string &json_data);
};

}  // namespace emontx
}  // namespace esphome
