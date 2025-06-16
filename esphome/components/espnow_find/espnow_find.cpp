#if defined(USE_ESP32)
#include "espnow_find.h"
#include "esphome/core/automation.h"
#include "esphome/core/base_automation.h"
#ifdef USE_WIFI
#include "esphome/core/base_automation.h"

namespace esphome {
namespace espnow_find {

ESPNowFindPeer::ESPNowFindPeer(/* args */) {}

ESPNowFindPeer::~ESPNowFindPeer() {}

void ESPNowFindPeer::initialize() {
  this->parent_->set_trigger_for(ESPNOW_FIND_IDENTIFIER, 0, TRIGGER_ON_SUCCEED, &ESPNowFindPeer::find_peer_succeed_);
  this->parent_->set_trigger_for(ESPNOW_FIND_IDENTIFIER, 0, TRIGGER_ON_TIMEOUT, &ESPNowFindPeer::find_peer_failed_);
}

ESPNowFindPeerState ESPNowFindPeer::find_peer(uint64_t peer) {
#ifdef USE_WIFI
  if (global_wifi_component != nullptr && global_wifi_component->is_connected()) {
    ESP_LOGE(TAG, "Manual changing the channel is not possible with WIFI connected.");
    this->mark_failed();
    return FIND_PEER_BUSY;
  }
#endif
  uint8_t data = 1;
  if (peer == 0ul) {
    this->find_mac_address_ = 0ul;
    this->find_peer_state_ = FIND_PEER_NONE;
    if (this->find_start_channel_ != 0) {
      this->parent_->set_wifi_channel(this->find_start_channel_);
    }
  } else if (this->find_mac_address_ != 0 && this->find_mac_address_ != peer) {
    return FIND_PEER_BUSY;
  } else if (this->find_peer_state_ == FIND_PEER_NONE) {
    this->find_start_channel_ = this->parent_->get_wifi_channel();
    this->find_mac_address_ = peer;
    std::shared_ptr<ESPNowPacket> packet =
        this->parent_->make_packet(this->find_mac_address_, &data, 0, ESPNOW_FIND_IDENTIFIER, 0);
    packet->options(OPTION_SEND_DIRECT, true);
    this->parent_->send(packet);
    this->find_peer_state_ = FIND_PEER_SCANNING;
  }
  return this->find_peer_state_;
}

void ESPNowFindPeer::find_peer_succeed_(std::shared_ptr<ESPNowPacket> packet) {
  if (this->find_peer_state_ != FIND_PEER_NONE) {
    this->find_peer_state_ = FIND_PEER_SUCCEED;
    this->find_start_channel_ = 0;
    this->find_mac_address_ = 0ul;
  }
}

void ESPNowFindPeer::find_peer_failed_(std::shared_ptr<ESPNowPacket> packet) {
  if (this->find_peer_state_ != FIND_PEER_NONE) {
    uint8_t channel = this->parent_->get_wifi_channel() + 1;
    if (channel > 14) {
      channel = 1;
    }
    if (channel == this->find_start_channel_) {
      this->find_peer_state_ = FIND_PEER_FAILED;
      this->find_start_channel_ = 0;
      this->find_mac_address_ = 0ul;
    } else {
      this->parent_->set_wifi_channel(channel);
      uint8_t data = 1;
      std::shared_ptr<ESPNowPacket> packet =
          this->parent_->make_packet(this->find_mac_address_, &data, 0, ESPNOW_FIND_IDENTIFIER, 0);
      packet->options(OPTION_SEND_DIRECT, true);
      this->parent_->send(packet);
    }
  }
}

}  // namespace espnow_find
}  // namespace esphome

#endif
