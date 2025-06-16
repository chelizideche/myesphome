#pragma once

#if defined(USE_ESP32)

#include "esphome/core/automation.h"
#include "esphome/core/base_automation.h"
#include "esphome/components/espnow/espnow.h"

namespace esphome {
namespace espnow_find {

static const uint32_t ESPNOW_FIND_IDENTIFIER = 0x4664;  // = Fd

enum ESPNowFindPeerState : uint8_t {
  FIND_PEER_NONE = 0,
  FIND_PEER_SCANNING = 1,
  FIND_PEER_SUCCEED = 2,
  FIND_PEER_FAILED = 4,
  FIND_PEER_BUSY = 5,
};

class ESPNowFindPeer : public ESPNowApp {
 public:
  ESPNowFindPeer(/* args */);
  ~ESPNowFindPeer();
  void initialize() override;

  ESPNowFindPeerState find_peer(uint64_t peer);
  uint16_t application() override { return ESPNOW_FIND_IDENTIFIER; }

 private:
  /* data */
  uint64_t find_mac_address_{0};
  uint64_t find_start_channel_{0};
  ESPNowFindPeerState find_peer_state_{FIND_PEER_NONE};
  void find_peer_succeed_(std::shared_ptr<ESPNowPacket> packet);
  void find_peer_failed_(std::shared_ptr<ESPNowPacket> packet);
};

/*********************************  Actions **************************************/
template<typename... Ts>
class FindPeerAction : public Action<Ts...>, public Parented<ESPNowFindPeer>, public Component {
 public:
  TEMPLATABLE_VALUE(uint64_t, mac_address);

  void add_on_succeed(const std::vector<Action<Ts...> *> &actions) {
    this->success_.add_actions(actions);
    this->success_.add_action(new LambdaAction<Ts...>([this](Ts... x) { this->play_next_(x...); }));
  }

  void add_on_failed(const std::vector<Action<Ts...> *> &actions) {
    this->failed_.add_actions(actions);
    this->failed_.add_action(new LambdaAction<Ts...>([this](Ts... x) { this->play_next_(x...); }));
  }

  void loop() override {
    if (this->num_running_ == 0)
      return;
    if (this->peer_ == 0ul)
      return;

    ESPNowFindPeerState state_ = this->parent_->find_peer(this->peer_);
    if (state_ == FIND_PEER_SCANNING) {
      return;
    }

    if (state_ == FIND_PEER_SUCCEED && !this->success_.empty()) {
      this->success_.play_tuple(this->var_);
    } else if (state_ == FIND_PEER_FAILED && !this->failed_.empty()) {
      this->failed_.play_tuple(this->var_);
    } else {
      this->play_next_tuple_(this->var_);
    }
    this->peer_ = 0ul;
  }

  void play_complex(Ts... x) override {
    if (this->mac_address_.has_value()) {
      this->peer_ = this->mac_address_.value(x...);
    } else {
      this->peer_ = this->parent_->get_default_mac_address();
    }
    ESPNowFindPeerState state_ = this->parent_->find_peer(this->peer_);
    if (state_ == FIND_PEER_SCANNING) {
      this->num_running_++;
      this->var_ = std::make_tuple(x...);
    } else {
      this->peer_ = 0ul;
    }
  }

  float get_setup_priority() const override { return setup_priority::DATA; }

  void play(Ts... x) override {}  // ignore - see play_complex

  void stop() override {
    this->peer_ = 0ul;
    this->parent_->find_peer(0ul);
    this->failed_.stop();
    this->success_.stop();
  }

 protected:
  std::tuple<Ts...> var_{};
  ActionList<Ts...> success_;
  ActionList<Ts...> failed_;
  uint64_t peer_{0};
};

}  // namespace espnow_find
}  // namespace esphome

#endif
