#include "emontx.h"
#include "esphome/core/log.h"
#include "esphome/components/json/json_util.h"

#ifdef USE_HTTP_REQUEST
#include "esphome/components/network/util.h"
#endif

namespace esphome {
namespace emontx {

static const char *const TAG = "emontx";

/**
 * @brief Initializes the EmonTx.
 */
void EmonTx::setup() {
  ESP_LOGCONFIG(TAG, "Setting up EmonTx component");

#ifdef USE_SENSOR
  // Log sensors at setup time
  ESP_LOGCONFIG(TAG, "Currently registered sensors: %u", this->sensors_.size());
  for (const auto &sensor_pair : this->sensors_) {
    ESP_LOGCONFIG(TAG, "  Sensor '%s' registered", sensor_pair.first.c_str());
  }
#else
  ESP_LOGCONFIG(TAG, "Sensor support: DISABLED");
#endif

#ifdef USE_HTTP_REQUEST
  // Initialize HTTP client if EmonCMS is configured
  if (has_emoncms_config_) {
    ESP_LOGCONFIG(TAG, "EmonCMS forwarding enabled:");
    ESP_LOGCONFIG(TAG, "  Server: %s", this->emoncms_server_.c_str());
    ESP_LOGCONFIG(TAG, "  Node: %s", this->emoncms_node_.c_str());
    ESP_LOGCONFIG(TAG, "  API Key: %s...", this->emoncms_apikey_.substr(0, 5).c_str());

    // Pre-build the URL - only done once
    std::string url = emoncms_server_;
    if (url.back() != '/') {
      url += '/';
    }
    url += "input/post";
    this->emoncms_url_ = url;

    // Pre-build the body prefix - only done once
    std::string body_prefix = "node=";
    body_prefix += emoncms_node_;
    body_prefix += "&apikey=";
    body_prefix += emoncms_apikey_;
    body_prefix += "&data=";
    this->emoncms_body_prefix_ = body_prefix;

    ESP_LOGD(TAG, "EmonCMS URL constructed: %s", this->emoncms_url_.c_str());
  }
#endif

#ifdef USE_MQTT_FORWARD
  // Log MQTT forwarding configuration
  if (has_mqtt_config_) {
    ESP_LOGCONFIG(TAG, "MQTT forwarding enabled:");
    ESP_LOGCONFIG(TAG, "  Topic prefix: %s", this->mqtt_topic_prefix_.c_str());

    mqtt::global_mqtt_client->disable_discovery();
    mqtt::global_mqtt_client->set_topic_prefix(this->mqtt_topic_prefix_);

    ESP_LOGCONFIG(TAG, "  Global MQTT discovery: %s",
                  mqtt::global_mqtt_client->is_discovery_enabled() ? "ENABLED" : "DISABLED");
  }
#endif
}

/**
 * @brief Updates the EmonTx state. Resets the buffer index and transitions the state from OFF to ON.
 */
void EmonTx::update() {
  ESP_LOGD(TAG, "Updating EmonTx state...");
  buffer_.clear();  // Clear the buffer for new data
  ESP_LOGD(TAG, "EmonTx is now ready to receive data.");
}

/**
 * @brief Implements the main state machine for processing incoming data.
 *
 * @details The state machine transitions through the following states:
 * - OFF: Does nothing.
 * - ON: Reads characters until the start frame (0x2) is found.
 * - START_FRAME_RECEIVED: Reads characters until the end frame (0x3) is found.
 * - END_FRAME_RECEIVED: Processes the buffer to extract groups, validate CRC, and publish values.
 */
void EmonTx::loop() {
  while (available()) {
    char c = read();

    if (c == '\n' || c == '\r') {
      if (!buffer_.empty()) {
        parse_json_(buffer_);

#ifdef USE_HTTP_REQUEST
        // Forward to EmonCMS if configured
        if (has_emoncms_config_ && network::is_connected()) {
          send_to_emoncms_(buffer_);
        }
#endif

#ifdef USE_MQTT_FORWARD
        // Forward to MQTT if configured
        if (has_mqtt_config_ && mqtt::global_mqtt_client != nullptr && mqtt::global_mqtt_client->is_connected()) {
          send_to_mqtt_(buffer_);
        }
#endif
        buffer_.clear();
      }
    } else {
      buffer_ += c;
      // Prevent buffer overflow
      if (buffer_.length() > 1024) {
        ESP_LOGW(TAG, "Buffer overflow, clearing buffer");
        buffer_.clear();
      }
    }
  }
}

void EmonTx::parse_json_(const std::string &data) {
  ESP_LOGV(TAG, "Parsing JSON: %s", data.c_str());

  bool success = json::parse_json(data, [this](JsonObject root) {
#ifdef USE_SENSOR
    // Update all registered sensors
    for (auto &sensor_pair : this->sensors_) {
      const std::string &tag = sensor_pair.first;
      sensor::Sensor *sensor = sensor_pair.second;

      if (root.containsKey(tag)) {
        float value = root[tag];
        ESP_LOGD(TAG, "Updating sensor '%s' with value: %.2f", tag.c_str(), value);
        sensor->publish_state(value);
      }
    }
#endif

    // Also update all listeners
    for (auto *listener : this->emontx_listeners_) {
      if (root.containsKey(listener->tag)) {
        const auto value = root[listener->tag].as<std::string>();
        listener->publish_val(value);
      }
    }

    return true;  // Parsing was handled successfully
  });

  if (!success) {
    ESP_LOGW(TAG, "Failed to parse JSON");
  }
}

#ifdef USE_HTTP_REQUEST
void EmonTx::send_to_emoncms_(const std::string &json_data) {
  if (!has_emoncms_config_) {
    return;
  }

  if (!network::is_connected()) {
    ESP_LOGW(TAG, "Network not connected, skipping EmonCMS update");
    return;
  }

  ESP_LOGV(TAG, "Sending data to EmonCMS: %s", json_data.c_str());

  // Use the pre-built body prefix and just append the JSON data
  // This avoids rebuilding the same strings every time
  std::string body = this->emoncms_body_prefix_ + json_data;

  ESP_LOGV(TAG, "EmonCMS request to URL: %s", this->emoncms_url_.c_str());
  ESP_LOGV(TAG, "EmonCMS POST data: %s", body.c_str());

  // Create headers list with Content-Type
  std::list<http_request::Header> headers = {{"Content-Type", "application/x-www-form-urlencoded"}};

  // Send POST request and capture the response container
  auto container = http_client_->post(this->emoncms_url_, body, headers);

  // Check if request was successfully initiated
  if (container != nullptr) {
    ESP_LOGI(TAG, "HTTP POST request to EmonCMS initiated successfully");

    // Access status code from the container
    int status_code = container->status_code;

    if (status_code >= 200 && status_code < 300) {
      ESP_LOGI(TAG, "EmonCMS data sent successfully (HTTP %d)", status_code);

      // Check if there's content to read
      if (container->content_length > 0) {
        // Read response body in chunks
        const size_t buffer_size = 128;
        uint8_t buffer[buffer_size];
        std::string response;

        int bytes_read;
        while ((bytes_read = container->read(buffer, buffer_size)) > 0) {
          response.append(reinterpret_cast<char *>(buffer), bytes_read);
        }

        if (!response.empty()) {
          ESP_LOGV(TAG, "Response: %s", response.c_str());
        }
      }
    } else {
      ESP_LOGW(TAG, "Error sending data to EmonCMS (HTTP %d)", status_code);
    }

    // Always end the connection when done
    container->end();
  } else {
    ESP_LOGW(TAG, "Failed to initiate HTTP POST request to EmonCMS");
  }
}
#endif

#ifdef USE_MQTT_FORWARD
void EmonTx::send_to_mqtt_(const std::string &json_data) {
  if (!has_mqtt_config_ || mqtt::global_mqtt_client == nullptr || !mqtt::global_mqtt_client->is_connected()) {
    return;
  }

  // Use the topic prefix + /data for the raw JSON data
  std::string topic = mqtt_topic_prefix_;
  if (topic.back() != '/') {
    topic += '/';
  }
  topic += "data";

  // Send the raw JSON data
  ESP_LOGV(TAG, "Publishing to MQTT topic %s: %s", topic.c_str(), json_data.c_str());
  mqtt::global_mqtt_client->publish(topic, json_data, 0, false);

  ESP_LOGI(TAG, "Data published to MQTT topic %s", topic.c_str());

  // Optionally, also publish individual values to separate topics for easier consumption
  bool success = json::parse_json(json_data, [this](JsonObject root) {
    // For each key-value pair in the JSON
    for (JsonPair kv : root) {
      // Create topic for this value: prefix/key
      std::string value_topic = mqtt_topic_prefix_;
      if (value_topic.back() != '/') {
        value_topic += '/';
      }
      value_topic += kv.key().c_str();

      // Convert value to string
      std::string value;
      if (kv.value().is<float>() || kv.value().is<int>()) {
        value = to_string(kv.value().as<float>());
      } else if (kv.value().is<const char *>()) {
        value = kv.value().as<const char *>();
      } else {
        value = "unknown";
      }

      // Publish this value
      ESP_LOGV(TAG, "  Publishing %s = %s", kv.key().c_str(), value.c_str());
      mqtt::global_mqtt_client->publish(value_topic, value, 0, false);
    }
    return true;
  });

  if (!success) {
    ESP_LOGW(TAG, "Failed to parse JSON for MQTT individual topics");
  }
}
#endif

/**
 * @brief Dumps the EmonTx configuration to the log.
 *
 * @note Logs the UART settings and other configuration details.
 */
void EmonTx::dump_config() {
  ESP_LOGCONFIG(TAG, "EmonTx:");

#ifdef USE_SENSOR
  ESP_LOGCONFIG(TAG, "  Registered sensors: %u", this->sensors_.size());

  // List all registered sensors with their tags
  for (const auto &sensor_pair : this->sensors_) {
    ESP_LOGCONFIG(TAG, "  Sensor: %s", sensor_pair.first.c_str());
  }
#else
  ESP_LOGCONFIG(TAG, "  Sensor support: DISABLED");
#endif

#ifdef USE_HTTP_REQUEST
  // Show EmonCMS configuration
  if (has_emoncms_config_) {
    ESP_LOGCONFIG(TAG, "  EmonCMS Forwarding: ENABLED");
    ESP_LOGCONFIG(TAG, "    Server: %s", this->emoncms_server_.c_str());
    ESP_LOGCONFIG(TAG, "    Node: %s", this->emoncms_node_.c_str());
    ESP_LOGCONFIG(TAG, "    API Key: %s...", this->emoncms_apikey_.substr(0, 5).c_str());
  } else {
    ESP_LOGCONFIG(TAG, "  EmonCMS Forwarding: DISABLED");
  }
#else
  ESP_LOGCONFIG(TAG, "  EmonCMS Forwarding: DISABLED");
#endif

#ifdef USE_MQTT_FORWARD
  // Show MQTT forwarding configuration
  if (has_mqtt_config_) {
    ESP_LOGCONFIG(TAG, "  MQTT Forwarding: ENABLED");
    ESP_LOGCONFIG(TAG, "    Topic prefix: %s", mqtt::global_mqtt_client->get_topic_prefix().c_str());
    ESP_LOGCONFIG(TAG, "    Global MQTT discovery: %s",
                  mqtt::global_mqtt_client->is_discovery_enabled() ? "ENABLED" : "DISABLED");
  } else {
    ESP_LOGCONFIG(TAG, "  MQTT Forwarding: DISABLED");
  }
#else
  ESP_LOGCONFIG(TAG, "  MQTT Forwarding: NOT AVAILABLE");
#endif
}

/**
 * @brief Registers a listener to receive updates for specific tags.
 *
 * @param listener Pointer to the listener to register.
 */
#ifdef USE_SENSOR
void EmonTx::register_emontx_listener(EmonTxListener *listener) { emontx_listeners_.push_back(listener); }

void EmonTx::register_sensor(const std::string &tag_name, sensor::Sensor *sensor) {
  ESP_LOGCONFIG(TAG, "Registering sensor for tag: %s", tag_name.c_str());
  this->sensors_[tag_name] = sensor;
}
#endif
}  // namespace emontx
}  // namespace esphome
