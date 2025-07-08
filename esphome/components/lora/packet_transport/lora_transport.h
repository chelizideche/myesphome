#pragma once

#include "esphome/core/component.h"
#include "esphome/components/lora/lora.h"
#include "esphome/components/packet_transport/packet_transport.h"
#include <vector>

namespace esphome {
namespace lora {

class LoRaTransport : public packet_transport::PacketTransport, public Parented<LoRa>, public LoRaListener {
 public:
  void update() override;
  void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  void send_packet(const std::vector<uint8_t> &buf) const override;
  bool should_send() override { return true; }
  size_t get_max_packet_size() override { return this->parent_->get_max_packet_size(); }
};

}  // namespace lora
}  // namespace esphome
