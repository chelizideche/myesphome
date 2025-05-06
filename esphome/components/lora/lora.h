#pragma once
#include "esphome/core/component.h"

#include <vector>

namespace esphome {
namespace lora {

class LoRaListener {
 public:
  virtual void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr);
};

class LoRa : public Component {
 public:
  // EspHome default functions
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // Config setter functions
  void set_platform_name(const char *name) { this->platform_name_ = name; }
  virtual void set_frequency(uint32_t frequency) = 0;

  // Operational functions
  void packet_received(const std::vector<uint8_t> &packet, float rssi, float snr);
  virtual void send_packet(const std::vector<uint8_t> &buf) const = 0;

  // Listener functions
  void register_listener(LoRaListener *listener) { this->listeners_.push_back(listener); }
  Trigger<std::vector<uint8_t>, float, float> *get_packet_trigger() const { return this->packet_trigger_; };

 protected:
  const char *platform_name_{""};

  std::vector<LoRaListener *> listeners_;
  void call_listeners_(const std::vector<uint8_t> &packet, float rssi, float snr);
  Trigger<std::vector<uint8_t>, float, float> *packet_trigger_{new Trigger<std::vector<uint8_t>, float, float>()};
};

}  // namespace lora
}  // namespace esphome