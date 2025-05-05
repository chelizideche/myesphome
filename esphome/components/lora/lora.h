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
  void packet_received(const std::vector<uint8_t> &packet, float rssi, float snr);
  virtual void send_packet(std::vector<uint8_t> &buf) const = 0;
  void register_listener(LoRaListener *listener) { this->listeners_.push_back(listener); }
  Trigger<std::vector<uint8_t>, float, float> *get_packet_trigger() const { return this->packet_trigger_; };

 protected:
  void call_listeners_(const std::vector<uint8_t> &packet, float rssi, float snr);
  std::vector<LoRaListener *> listeners_;
  Trigger<std::vector<uint8_t>, float, float> *packet_trigger_{new Trigger<std::vector<uint8_t>, float, float>()};
};

}  // namespace lora
}  // namespace esphome