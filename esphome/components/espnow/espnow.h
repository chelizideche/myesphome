#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/base_automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "esp_mac.h"
#include <esp_now.h>

#include <algorithm>  // std::max
#include <map>
#include <memory>
#include <string>
#include <vector>

#if defined(USE_ESP32)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 1)

namespace esphome {
namespace espnow {

static const uint64_t ESPNOW_BROADCAST_ADDR = 0xFFFFFFFFFFFF;
static const uint64_t ESPNOW_MULTICAST_ADDR = 0xFFFFFFFFFFFE;

static const uint8_t ESPNOW_HEADER_SIZE = 8;

static const uint8_t ESPNOW_MAX_DATA_SIZE = ESP_NOW_MAX_DATA_LEN - ESPNOW_HEADER_SIZE;

static const uint16_t ESPNOW_DEFAULT_APP_ID = 0x7453;  // = St

static const uint8_t ESPNOW_COMMAND_ACK = 254;
static const uint8_t ESPNOW_COMMAND_NACK = 253;
static const uint8_t ESPNOW_COMMAND_RESEND = 252;
static const uint8_t ESPNOW_COMMAND_PING = 251;
static const uint8_t ESPNOW_COMMAND_PONG = 250;

static const uint64_t FAILED = 0;

class ESPNowPacket;
class ESPNowComponent;

class ESPNowTAG {
 public:
  // could be made inline with C++17
  static const char *const TAG;
};

void show_packet(const std::string &title, ESPNowPacket *packet, bool extended = false);
std::string peer_string(uint64_t peer);
uint64_t make_key(uint64_t peer, uint16_t sequents);

enum ESPNowTriggers : uint8_t {
  TRIGGER_NONE = 0,
  TRIGGER_ON_RECEIVE = 2,
  TRIGGER_ON_BROADCAST = 3,
  TRIGGER_ON_SUCCEED = 10,
  TRIGGER_ON_FAILED = 20,
  TRIGGER_ON_TIMEOUT = 22,
  TRIGGER_ON_RAW_DATA = 30
};

enum ESPNowOptions : uint8_t {
  OPTION_RECEIVED = 0,
  OPTION_BROADCAST,
  OPTION_MULTICAST,
  OPTION_SEND_DIRECT,
  OPTION_RAW,

  OPTION_DONT_WAIT,

  OPTION_BEEN_SEND,
  OPTION_FINISHED
};

enum ESPNowAppMode { PM_UNIVERSAL, PM_COLLECTOR, PM_PROVIDER };

using EPSNowTriggerCallback = std::function<void(std::shared_ptr<ESPNowPacket>)>;

class ESPNowPacket {
 public:
  inline ESPNowPacket() ESPHOME_ALWAYS_INLINE {}
  // Create packet to be send.
  inline ~ESPNowPacket() ESPHOME_ALWAYS_INLINE {
    esph_log_e(ESPNowTAG::TAG, "%s| Deconstruct packet.", this->info().c_str());
  }

  ESPNowPacket(uint16_t app_id, uint8_t command, uint64_t peer, const uint8_t *payload, size_t size);
  ESPNowPacket(uint64_t peer, const uint8_t *payload, size_t size);
  ESPNowPacket(const uint8_t *mac_address, const uint8_t *payload, int size);

  uint64_t key() const { return make_key(this->peer(), this->sequents()); }

  size_t header_size() const {
    if (this->options(OPTION_RAW) || this->content_.size() < ESPNOW_HEADER_SIZE) {
      return 0;
    } else {
      return ESPNOW_HEADER_SIZE;
    }
  }
  size_t content_size() const { return this->content_.size(); }
  size_t payload_size() const { return this->content_size() - this->header_size(); }

  uint8_t *content() const { return (uint8_t *) this->content_.data(); }

  uint8_t *payload() const { return (uint8_t *) this->content_.data() + this->header_size(); }

  void peer(uint64_t value) { this->peer_ = value; }
  uint64_t peer() const { return this->peer_; }

  void mac_address(const uint8_t *value) { memcpy((void *) this->mac_address(), (const void *) value, 6); };
  uint8_t *mac_address() const { return (uint8_t *) &(this->peer_); }
  bool is_mac_address(const uint8_t *value) const { return memcmp(value, this->mac_address(), 6) == 0; }

  std::string peer_str() const;

  std::string info() const;

  uint16_t application() const {
    if (this->header_size() == 0) {
      return 0;
    } else {
      return this->read16h(2);
    }
  }

  uint8_t command() const {
    if (this->header_size() == 0) {
      return 0;
    } else {
      return this->options(OPTION_RAW) ? 0 : this->read8h(4);
    }
  }

  void sequents(uint16_t value) {
    this->sequents_ = value;
    if (!this->options(OPTION_RAW)) {
      this->write16h(5, value);
    }
  }
  uint16_t sequents() const { return this->options(OPTION_RAW) ? this->sequents_ : this->read16h(5); }

  void timestamp(uint32_t value) { this->timestamp_ = value; }
  uint32_t timestamp() const { return this->timestamp_; }

  void rssi(uint8_t value) { this->rssi_ = value; }
  uint8_t rssi() const { return this->rssi_; }

  uint8_t attempt() const { return this->attempt_; }
  void retry() { this->attempt_++; }

  void status(ESPNowTriggers value) { this->status_ = value; }
  ESPNowTriggers status() const { return this->status_; }

  void trigger_group(uint32_t value) { this->trigger_group_ = value; }
  uint32_t trigger_group() { return this->trigger_group_; }

  void options(ESPNowOptions option, bool value) {
    this->options_ = (this->options_ & ~((uint16_t) 1 << option)) | ((uint16_t) value << option);
  }
  bool options(ESPNowOptions option) const { return this->options_ & ((uint16_t) 1 << option); }
  uint16_t options() const { return this->options_; }

  bool is_valid() const;

  void write(size_t offset, size_t length, uint64_t value);
  uint64_t read(size_t offset, size_t length) const;

  void write8u(size_t offset, uint8_t value) { this->write(offset, sizeof(uint8_t), value); };
  uint8_t read8u(size_t offset) const { return static_cast<uint8_t>(this->read(offset, sizeof(uint8_t))); };

  void write16u(size_t offset, uint16_t value) { this->write(offset, sizeof(uint16_t), value); }
  uint16_t read16u(size_t offset) const { return static_cast<uint16_t>(this->read(offset, sizeof(uint16_t))); };

  void write64u(size_t offset, uint16_t value) { this->write(offset, sizeof(uint64_t), value); }
  uint64_t read64u(size_t offset) const { return this->read(offset, sizeof(uint64_t)); };

  void write8h(size_t pos, uint8_t value) { this->content_[pos] = value; };
  uint8_t read8h(size_t pos) const { return this->content_[pos]; };
  void write16h(size_t pos, uint16_t value) {
    this->content_[pos++] = value >> 8;
    this->content_[pos] = value;
  }
  uint16_t read16h(size_t pos) const {
    return ((uint16_t) (this->content_[pos++]) << 8) + (uint16_t) this->content_[pos];
  }

 protected:
  friend class ESPNowComponent;
  uint64_t peer_{0};
  uint8_t rssi_{0};
  uint16_t options_{0};
  uint8_t attempt_{0};
  uint32_t timestamp_{0};
  uint16_t sequents_{0};
  uint32_t trigger_group_{0};

  ESPNowTriggers status_{TRIGGER_NONE};

  std::vector<uint8_t> content_{};
};

class ESPNowComponent;

class ESPNowApp : public Parented<ESPNowComponent> {
 public:
  void set_app_mode(ESPNowAppMode mode) { this->app_mode_ = mode; }
  ESPNowAppMode get_app_mode() { return this->app_mode_; }

 protected:
  ESPNowAppMode app_mode_{PM_UNIVERSAL};

 public:
  virtual uint16_t application() = 0;
  virtual void init_app() {}

  virtual void on_add_peer(uint64_t peer){};
  virtual void on_del_peer(uint64_t peer){};

  virtual bool is_paired(uint64_t to_peer) { return true; }

  bool send(uint64_t peer, const uint8_t *data, uint8_t len, uint8_t command = 0);

 protected:
  std::string get_mode_name_() {
    switch (this->app_mode_) {
      case PM_UNIVERSAL:
        return "Universal";
      case PM_COLLECTOR:
        return "Collector";
      case PM_PROVIDER:
        return "Provider";
    }
  }
};

class ESPNowComponent : public Component {
 public:
  ESPNowComponent();

  void dump_config() override;

  float get_setup_priority() const override { return -100; }

  void set_wifi_channel(uint8_t channel);
  uint8_t get_wifi_channel() { return this->wifi_channel_; }

  void set_auto_add_peer(bool value) { this->auto_add_peer_ = value; }
  void set_wait_for_ack(bool value) { this->wait_for_ack_ = value; }
  void set_auto_channel_scan(bool value) { this->auto_channel_scan_ = value; }

  void set_confirmation_timeout(uint32_t timeout) { this->confirmation_timeout_ = timeout; }
  void set_attempts(uint8_t value) { this->attempts_ = value; }
  void set_pairing_app(ESPNowApp *pairing_app) { this->pairing_app_ = pairing_app; }
  void set_default_app_id(uint16_t value) {}
  void set_trigger_for(uint32_t app_id, uint8_t command, ESPNowTriggers event, EPSNowTriggerCallback &&cb) {
    this->triggers_[app_id][command][event] = std::move(cb);
  }
  uint64_t get_own_peer_address() const { return this->own_peer_address_; }

  Trigger<std::shared_ptr<ESPNowPacket>> *get_new_peer_trigger() const { return this->new_peer_trigger_; }
  Trigger<std::shared_ptr<ESPNowPacket>> *get_raw_data_trigger() const { return this->raw_data_trigger_; }

  void set_suspension(bool value) { this->is_suspended_ = value; }
  bool is_suspension() { return this->is_suspended_; }

  void setup() override;
  void loop() override;
  bool can_proceed() override;

  bool is_paired(uint64_t to_peer);
  std::shared_ptr<ESPNowPacket> make_packet(uint64_t peer, const uint8_t *data, uint8_t size, uint16_t app_id,
                                            uint8_t command) {
    return std::make_shared<ESPNowPacket>(app_id, command, peer, data, size);
  }

  std::shared_ptr<ESPNowPacket> make_packet(uint64_t peer, const uint8_t *data, uint8_t size) {
    return std::make_shared<ESPNowPacket>(peer, data, size);
  }
  std::shared_ptr<ESPNowPacket> get_packet(uint64_t key) { return this->packet_send_map_[key]; }

  bool send(std::weak_ptr<ESPNowPacket> weak_packet);
  bool send_system_command(std::weak_ptr<ESPNowPacket> weak_packet, uint8_t command);

  void remove_packet(uint64_t key);

  void register_app(ESPNowApp *app) {
    app->set_parent(this);
    this->apps_[app->application()] = app;
    app->init_app();
  }

  esp_err_t add_peer(uint64_t peer);
  esp_err_t del_peer(uint64_t peer);
  size_t send_queue_used() { return uxQueueMessagesWaiting(this->send_queue_); }

  static void espnow_task(void *params);

  uint16_t get_next_sequents(uint64_t peer, uint16_t app);
  bool is_valid_squence(uint64_t peer, uint16_t app, uint16_t received_sequence);

  static void on_data_received(const esp_now_recv_info_t *recv_info, const uint8_t *data, int size);

 protected:
  void on_data_received_(const esp_now_recv_info_t *recv_info, const uint8_t *data, int size);
  bool validate_channel_(uint8_t channel);
  void handle_system_command_(std::weak_ptr<ESPNowPacket> weak_packet, uint8_t command);
  void handle_ack_command_(std::weak_ptr<ESPNowPacket> weak_packet);

  //  ESPNowApp *get_app_(uint16_t app);
  ESPNowApp *pairing_app_{nullptr};

  bool is_suspended_{false};

  uint64_t own_peer_address_{0};
  uint64_t default_peer_address_{0};

  uint8_t wifi_channel_{0};
  uint32_t confirmation_timeout_{5000};
  uint8_t attempts_{1};
  uint64_t last_packet_send_{0ull};

  bool auto_add_peer_{false};
  bool wait_for_ack_{true};
  bool auto_channel_scan_{false};

  EPSNowTriggerCallback get_trigger_for_(ESPNowTriggers event, std::weak_ptr<ESPNowPacket> weak_packet);
  void call_trigger_for_(ESPNowTriggers event, std::weak_ptr<ESPNowPacket> weak_packet);

  void call_on_add_peer_(uint64_t peer);
  void call_on_del_peer_(uint64_t peer);

  void start_multi_cast_();

  QueueHandle_t send_queue_{};

  Trigger<std::shared_ptr<ESPNowPacket>> *new_peer_trigger_ = new Trigger<std::shared_ptr<ESPNowPacket>>();
  Trigger<std::shared_ptr<ESPNowPacket>> *raw_data_trigger_ = new Trigger<std::shared_ptr<ESPNowPacket>>();

  std::map<uint32_t, std::map<uint8_t, std::map<ESPNowTriggers, EPSNowTriggerCallback>>> triggers_;
  std::map<uint64_t, std::map<uint16_t, uint16_t>> next_sequents_{};
  std::map<uint64_t, std::shared_ptr<ESPNowPacket>> packet_send_map_{};
  std::map<uint32_t, ESPNowApp *> apps_{};
  std::vector<uint64_t> peers_{};
  std::vector<uint64_t> multicast_{};

  bool task_running_{false};
  static ESPNowComponent *static_;  // NOLINT
};

/*********************************  Actions **************************************/
template<typename... Ts> class SendAction : public Action<Ts...>, public Parented<ESPNowComponent> {
  TEMPLATABLE_VALUE(uint64_t, mac_address);
  TEMPLATABLE_VALUE(uint16_t, app_id);

  TEMPLATABLE_VALUE(uint8_t, command);
  TEMPLATABLE_VALUE(std::vector<uint8_t>, payload);

 public:
  void set_dont_wait_flag() { this->dont_wait_flag_ = true; }
  void set_raw_data() { this->raw_data_flag_ = true; }

  void set_trigger_group(uint32_t value) { this->trigger_group_ = value; }
  void play(Ts... x) override {
    std::vector<uint8_t> payload = this->payload_.value(x...);
    uint64_t mac = this->mac_address_.value(x...);
    uint16_t app = this->app_id_.value(x...);
    uint8_t command = this->command_.value(x...);
    std::shared_ptr<ESPNowPacket> packet;
    if (raw_data_flag_) {
      packet = this->parent_->make_packet(mac, payload.data(), payload.size());
    } else {
      packet = this->parent_->make_packet(mac, payload.data(), payload.size(), app, command);
    }
    packet->trigger_group(this->triggerGroup_);
    packet->options(OPTION_DONT_WAIT, this->dont_wait_flag_);
    this->parent_->send(packet);
  }

 protected:
  bool dont_wait_flag_{false};
  bool raw_data_flag_{false};
  uint32_t trigger_group_{0};
};

template<typename... Ts> class NewPeerAction : public Action<Ts...>, public Parented<ESPNowComponent> {
 public:
  TEMPLATABLE_VALUE(uint64_t, mac_address);
  void play(Ts... x) override {
    uint64_t mac_address = this->mac_address_.value(x...);
    this->parent_->add_peer(mac_address);
  }
};

template<typename... Ts> class DelPeerAction : public Action<Ts...>, public Parented<ESPNowComponent> {
 public:
  TEMPLATABLE_VALUE(uint64_t, mac_address);
  void play(Ts... x) override {
    uint64_t mac_address = this->mac_address_.value(x...);
    this->parent_->del_peer(mac_address);
  }
};

template<typename... Ts> class SetChannelAction : public Action<Ts...>, public Parented<ESPNowComponent> {
 public:
  TEMPLATABLE_VALUE(int8_t, channel);
  void play(Ts... x) override {
    int8_t value = this->channel_.value(x...);
    this->parent_->set_wifi_channel(value);
  }
};

/********************************* triggers **************************************/

class ESPNowDefaultTrigger : public Trigger<std::shared_ptr<ESPNowPacket>> {
 public:
  explicit ESPNowDefaultTrigger(ESPNowComponent *parent, uint32_t app, uint8_t command, ESPNowTriggers event) {
    auto cb_trigger = std::bind(&ESPNowDefaultTrigger::call_trigger, this, std::placeholders::_1);
    parent->set_trigger_for(app, command, event, cb_trigger);
  }
  void call_trigger(const std::weak_ptr<ESPNowPacket> weak_packet) { this->trigger(weak_packet.lock()); }
};

}  // namespace espnow
}  // namespace esphome

#endif
#endif
