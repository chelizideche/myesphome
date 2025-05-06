#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sx127x/sx127x.h"
#include "esphome/components/lora/lora.h"
#include <vector>

namespace esphome {
namespace sx127x {

class SX127xLoRa : public lora::LoRa, public Parented<SX127x>, public SX127xListener {
 public:
  void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) override {
    this->packet_received(packet, rssi, snr);
  }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void send_packet(const std::vector<uint8_t> &buf) const override { this->parent_->transmit_packet(buf); }
  void set_frequency(uint32_t frequency) override;

 protected:
  // size_t get_max_packet_size() override { return this->parent_->get_max_packet_size(); }
};

}  // namespace sx127x
}  // namespace esphome
