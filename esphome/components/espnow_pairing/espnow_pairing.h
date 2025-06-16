#pragma once

#if defined(USE_ESP32)

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/espnow/espnow.h"
#include "esphome/core/preferences.h"

#include <array>
#include <memory>
#include <queue>
#include <vector>
#include <mutex>
#include <map>

namespace esphome {
namespace espnow_pairing {

class ESPNowPairing : public pollingcomponent, public ESPNowProtocol {
 public:
  set_network_name(std::string name) { this->network_name_ = name; }
  set_private_key(std::string key) { this->private_key_ = key; }
  set_scan_timeout(uint32 timeout) { this->scan_timeout_ = timeout; }

 protected:
  std::string network_name_{"FreeNetNow"};
  std::string private_key_{"aSZvxD9Lo*2pqf"};
  uint32_t scan_timeout_{5000};

 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  void start();
  void stop();

  bool on_receive(const ESPNowPacket &packet) override { return false; };
  bool on_broadcast(const ESPNowPacket &packet) override { return false; };

  bool on_sent(const ESPNowPacket &packet, bool status) override { return false; };
  bool on_new_peer(const ESPNowPacket &packet) override { return false; };

  uint16_t get_protocol_id() = 0x5072;
  std::string get_protocol_name() = "EspNow Pairing";

 protected:
  std::map<uint64_t, uint8_t> pairings_{};
  uint64_t main_keeper_{0};
  bool scanning_{false};
  std::string random_code_{""};

  void start_scan_timer_();
  void update_active_keeper_();

  ESPPreferenceObject pref_;
}

}  // namespace espnow_pairing
}  // namespace esphome
