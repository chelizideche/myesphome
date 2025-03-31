#include "espnow.h"

#include <cstring>

#include "esp_mac.h"
#include "esp_random.h"

#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_event.h"

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif
#include "esphome/core/application.h"
#include "esphome/core/version.h"
#include "esphome/core/log.h"

#include <memory>

#if defined(USE_ESP32)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 1)

namespace esphome {
namespace espnow {

static const char *const TAG = "espnow";
const char *const ESPNowTAG::TAG = "espnow";

static const size_t SEND_BUFFER_SIZE = 200;

static const uint16_t TRANSPORT_HEADER = 0x4E77;  // {'N', 'w'};

ESPNowComponent *ESPNowComponent::static_{nullptr};  // NOLINT

uint64_t make_key(uint64_t peer, uint16_t sequents) { return ((peer & ESPNOW_BROADCAST_ADDR) << 16) + sequents; }

void show_packet(const std::string &title, ESPNowPacket *packet, bool extended) {
  if (packet == nullptr) {
    ESP_LOGE(TAG, ">>> %s ~~~ Packet is NOT SET <<<", title.c_str());
  } else {
    size_t size = packet->payload_size();
    if (!packet->is_valid()) {
      ESP_LOGV(TAG, "%s| %s, size: %d, Raw: %s", packet->info().c_str(), title.c_str(), packet->content_size(),
               packet->content());

    } else if (!extended || size == 0) {
      ESP_LOGV(TAG, "%s| %s", packet->info().c_str(), title.c_str());
    } else if (size == 1) {
      ESP_LOGV(TAG, "%s| %s | %02x                                        ", packet->info().c_str(), title.c_str(),
               packet->read8u(0));
    } else if (size == 2) {
      ESP_LOGV(TAG, "%s| %s | %02x%02x                                   ", packet->info().c_str(), title.c_str(),
               packet->read8u(0), packet->read8u(1));
    } else if (size == 3) {
      ESP_LOGV(TAG, "%s| %s | %02x%02x %02x                              ", packet->info().c_str(), title.c_str(),
               packet->read8u(0), packet->read8u(1), packet->read8u(2));
    } else if (size == 4) {
      ESP_LOGV(TAG, "%s| %s | %02x%02x %02x%02x                          ", packet->info().c_str(), title.c_str(),
               packet->read8u(0), packet->read8u(1), packet->read8u(2), packet->read8u(3));
    } else if (size == 5) {
      ESP_LOGV(TAG, "%s| %s | %02x%02x %02x%02x %02x                     ", packet->info().c_str(), title.c_str(),
               packet->read8u(0), packet->read8u(1), packet->read8u(2), packet->read8u(3), packet->read8u(4));
    } else if (size == 6) {
      ESP_LOGV(TAG, "%s| %s | %02x%02x %02x%02x %02x%02x                 ", packet->info().c_str(), title.c_str(),
               packet->read8u(0), packet->read8u(1), packet->read8u(2), packet->read8u(3), packet->read8u(4),
               packet->read8u(5));
    } else if (size == 7) {
      ESP_LOGV(TAG, "%s| %s | %02x%02x %02x%02x %02x%02x %02x             ", packet->info().c_str(), title.c_str(),
               packet->read8u(0), packet->read8u(1), packet->read8u(2), packet->read8u(3), packet->read8u(4),
               packet->read8u(5), packet->read8u(6));
    } else if (size == 8) {
      ESP_LOGV(TAG, "%s| %s | %02x%02x %02x%02x %02x%02x %02x%02x         ", packet->info().c_str(), title.c_str(),
               packet->read8u(0), packet->read8u(1), packet->read8u(2), packet->read8u(3), packet->read8u(4),
               packet->read8u(5), packet->read8u(6), packet->read8u(7));
    } else if (size == 9) {
      ESP_LOGV(TAG, "%s| %s | %02x%02x %02x%02x %02x%02x %02x%02x %02x    ", packet->info().c_str(), title.c_str(),
               packet->read8u(0), packet->read8u(1), packet->read8u(2), packet->read8u(3), packet->read8u(4),
               packet->read8u(5), packet->read8u(6), packet->read8u(7), packet->read8u(8));
    } else if (size >= 10) {
      ESP_LOGV(TAG, "%s| %s | %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x", packet->info().c_str(), title.c_str(),
               packet->read8u(0), packet->read8u(1), packet->read8u(2), packet->read8u(3), packet->read8u(4),
               packet->read8u(5), packet->read8u(6), packet->read8u(7), packet->read8u(8), packet->read8u(9));
    }
  }
}

std::string peer_string(uint64_t peer) {
  char mac[24];
  if (peer == 0) {
    snprintf(mac, sizeof(mac), "%s", "[Not Set]");
  } else if (peer == ESPNOW_BROADCAST_ADDR) {
    snprintf(mac, sizeof(mac), "%s", "[Broadcast]");
  } else if (peer == ESPNOW_MULTICAST_ADDR) {
    snprintf(mac, sizeof(mac), "%s", "Multicast]");
  } else {
    uint8_t *ppeer = (uint8_t *) &peer;
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", ppeer[0], ppeer[1], ppeer[2], ppeer[3], ppeer[4],
             ppeer[5]);
  }
  return mac;
}

/* ESPNowPacket ********************************************************************** */

ESPNowPacket::ESPNowPacket(uint16_t app_id, uint8_t command, uint64_t peer, const uint8_t *payload, size_t size) {
  if (size > ESPNOW_MAX_DATA_SIZE) {
    ESP_LOGE(TAG, "Payload size is too large. It should be less than %d instead it is %d", ESPNOW_MAX_DATA_SIZE, size);
    return;
  }
  if (peer == 0ull) {
    ESP_LOGE(TAG, "No Peer defined.");
    return;
  }
  if (app_id == 0ull) {
    ESP_LOGE(TAG, "No Application identifier defined.");
    return;
  }

  this->peer_ = peer;
  this->options(OPTION_BROADCAST, peer == ESPNOW_BROADCAST_ADDR);
  this->options(OPTION_MULTICAST, peer == ESPNOW_MULTICAST_ADDR);

  this->content_.clear();
  this->content_.resize(ESPNOW_HEADER_SIZE);

  this->write16h(0, TRANSPORT_HEADER);
  this->write16h(2, app_id);
  this->write8h(4, command);
  this->write8h(ESPNOW_HEADER_SIZE - 1, 255);
  if (size > 0) {
    this->content_.insert(this->content_.begin() + this->header_size(), payload, payload + size);
  }
}

ESPNowPacket::ESPNowPacket(uint64_t peer, const uint8_t *payload, size_t size) {
  if (size > ESP_NOW_MAX_DATA_LEN) {
    ESP_LOGE(TAG, "Payload size is to large. It should be less then %d instead it is %d", ESP_NOW_MAX_DATA_LEN, size);
    return;
  }
  if (peer == 0ull) {
    ESP_LOGE(TAG, "No Peer defined.");
    return;
  }
  ESP_LOGI(TAG, "Construction RAW DATA packet, size: %d", size);

  this->peer_ = peer;
  this->options(OPTION_BROADCAST, peer == ESPNOW_BROADCAST_ADDR);
  this->options(OPTION_MULTICAST, peer == ESPNOW_MULTICAST_ADDR);
  this->options(OPTION_RAW, true);

  this->content_.clear();
  this->content_.resize(size);
  if (size > 0) {
    this->content_.insert(this->content_.begin(), payload, payload + size);
  }
}

ESPNowPacket::ESPNowPacket(const uint8_t *peer, const uint8_t *payload, int size) {
  this->mac_address(peer);
  this->options(OPTION_RECEIVED, true);
  if (size > 0) {
    this->content_.insert(this->content_.begin(), payload, payload + size);
  }

  if (!this->is_valid()) {
    ESP_LOGI(TAG, "Construction received RAW packet, size: %d", this->content_size());
    this->options(OPTION_RAW, true);
  }
}

std::string ESPNowPacket::peer_str() const { return peer_string(this->peer_); }

std::string ESPNowPacket::info() const {
  char info[100];
  char model = this->options(OPTION_RECEIVED) ? 'R' : 'S';
  if (this->options(OPTION_RAW)) {
    snprintf(info, sizeof(info), "%s(%c%04x.%d)[%02x] RAW [%d] ", this->peer_str().c_str(), model, this->sequents(),
             this->options(), this->attempt(), this->payload_size());
  } else {
    snprintf(info, sizeof(info), "%s(%c%04x.%d)[%02x] %c%c-%02x[%d] ", this->peer_str().c_str(), model,
             this->sequents(), this->attempt(), this->options(), this->read8h(2), this->read8h(3), this->read8h(4),
             this->payload_size());
  }
  return info;
}

bool ESPNowPacket::is_valid() const {
  if (this->options(OPTION_RAW) || this->content_size() < ESPNOW_HEADER_SIZE) {
    return false;
  }
  bool header_valid = this->read16h(0) == TRANSPORT_HEADER;
  bool app_valid = (this->application() != 0);
  if (!header_valid || !app_valid) {
    ESP_LOGE(TAG, "Packet is invalid. Header is %s, App is %s.", header_valid ? "valid" : "invalid",
             app_valid ? "set" : "clear");
  }
  return header_valid && app_valid;
}

void ESPNowPacket::write(size_t pos, size_t length, uint64_t value) {
  auto index = pos + length + this->header_size();
  while (length-- != 0) {
    this->content_[--index] = static_cast<uint8_t>(value);
    value >>= 8;
  }
}

uint64_t ESPNowPacket::read(size_t offset, size_t length) const {
  offset += this->header_size();
  if (offset + length > this->content_.size()) {
    esph_log_e(ESPNowTAG::TAG, "Trying to read outside content size (%d of %d).", offset + length,
               this->content_.size());
    return 0;
  }
  uint64_t value = 0;
  offset += length;
  while (length-- != 0) {
    value <<= 8;
    value |= this->content_[--offset];
  }
  return value;
}

/* ESPNowComponent ********************************************************************** */

ESPNowComponent::ESPNowComponent() { ESPNowComponent::static_ = this; }  // NOLINT

void ESPNowComponent::dump_config() {
  uint32_t version = 0;
  esp_now_get_version(&version);

  ESP_LOGCONFIG(TAG, "esp_now:  V%d", version);
#ifdef USE_WIFI
  ESP_LOGE(TAG, "  WIFI enabled        : Yes. (Make sure provided device know how to channel swap)", this->attempts_);
  ESP_LOGW(TAG, "  Current Wifi channel: %d.", this->wifi_channel_);
#else
  ESP_LOGCONFIG(TAG, "  Wifi channel        : %d.", this->wifi_channel_);
#endif

  ESP_LOGCONFIG(TAG, "  Own Peer address    : %s.", peer_string(this->own_peer_address_).c_str());
  ESP_LOGCONFIG(TAG, "  Default Peer address: %s.", peer_string(this->own_peer_address_).c_str());

  ESP_LOGCONFIG(TAG, "  Auto add new peers  : %s.", this->auto_add_peer_ ? "Yes" : "No");
  ESP_LOGCONFIG(TAG, "  Wait for ACK        : %s.", this->wait_for_ack_ ? "Yes" : "No");
  ESP_LOGCONFIG(TAG, "  Convermation timeout: %" PRIx32 "ms.", this->confirmation_timeout_);
  ESP_LOGCONFIG(TAG, "  Max Send attempts   : %d.", this->attempts_);
}

void ESPNowComponent::setup() {
#ifndef USE_WIFI
  esp_event_loop_create_default();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_disconnect());

#ifdef CONFIG_ESPNOW_ENABLE_LONG_RANGE
  esp_wifi_get_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_LR);
#endif

  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(this->wifi_channel_, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
#else
  if (wifi::global_wifi_component != nullptr) {
    this->wifi_channel_ = wifi::global_wifi_component->get_wifi_channel();
  } else {
    ESP_LOGE(TAG, "Wifi Component is not setup correctly.");
    this->mark_failed();
  }
#endif

  esp_err_t err = esp_now_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  err = esp_now_register_recv_cb(ESPNowComponent::on_data_received);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_register_recv_cb failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  esp_wifi_get_mac(WIFI_IF_STA, (uint8_t *) &this->own_peer_address_);
  for (auto peer : this->peers_) {
    this->add_peer(peer);
  }

  this->send_queue_ = xQueueCreate(SEND_BUFFER_SIZE, sizeof(uint64_t));
  if (this->send_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create send queue");
    this->mark_failed();
    return;
  }
}

void ESPNowComponent::set_wifi_channel(uint8_t channel) {
#ifndef USE_WIFI
  if (this->is_ready() && (this->wifi_channel_ != channel)) {
    ESP_LOGI(TAG, "Channel is changed set to %d.", channel);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
  }
  this->wifi_channel_ = channel;
#else
  ESP_LOGE(TAG, "Manually changing the channel is not possible with WIFI enabled.");
  this->mark_failed();
#endif
}

bool ESPNowComponent::validate_channel_(uint8_t channel) {
  wifi_country_t g_self_country;
  esp_wifi_get_country(&g_self_country);
  if (channel >= g_self_country.schan + g_self_country.nchan) {
    ESP_LOGE(TAG, "Can't set channel %d, not allowed in country %c%c%c.", channel, g_self_country.cc[0],
             g_self_country.cc[1], g_self_country.cc[2]);
    return false;
  }
  return true;
}

void ESPNowComponent::loop() {
  if (!this->task_running_) {
    xTaskCreate(espnow_task, "espnow", 4096, this, tskIDLE_PRIORITY + 1, nullptr);
    this->task_running_ = true;
  }
#ifdef USE_WIFI
  int32_t new_channel = wifi::global_wifi_component->get_wifi_channel();
  if (new_channel != this->wifi_channel_) {
    ESP_LOGI(TAG, "Wifi Channel is changed from %d to %d.", this->wifi_channel_, new_channel);
    this->wifi_channel_ = new_channel;
  }
#endif
}

bool ESPNowComponent::can_proceed() {
#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr)
    return wifi::global_wifi_component->is_connected();
  return false;
#else
  return true;
#endif
}

void ESPNowComponent::on_data_received(const esp_now_recv_info_t *recv_info, const uint8_t *data, int size) {
  ESPNowComponent *that = ESPNowComponent::static_;
  if (that != nullptr) {
    that->on_data_received_(recv_info, data, size);
  }
}

void ESPNowComponent::on_data_received_(const esp_now_recv_info_t *recv_info, const uint8_t *data, int size) {
  auto packet = std::make_shared<ESPNowPacket>(recv_info->src_addr, data, size);  // NOLINT
  ESPNowTriggers event = TRIGGER_NONE;

  packet->options(OPTION_BROADCAST, ((uint64_t) *recv_info->des_addr & ESPNOW_BROADCAST_ADDR) == ESPNOW_BROADCAST_ADDR);
  packet->rssi(recv_info->rx_ctrl->rssi);
  packet->timestamp(recv_info->rx_ctrl->timestamp);

  if (packet->is_valid()) {
    if (!this->is_paired(packet->peer())) {
      ESP_LOGI(TAG, "Received packet from unknown device.");
      this->get_new_peer_trigger()->trigger(packet);
    }
    if (this->is_paired(packet->peer())) {
      if (packet->command() >= 250) {
        this->handle_system_command_(packet, packet->command());
        return;
      } else {
        event = (packet->options(OPTION_BROADCAST)) ? TRIGGER_ON_BROADCAST : TRIGGER_ON_RECEIVE;
      }
      show_packet("Packet Received", packet.get(), true);
      this->call_trigger_for_(event, packet);
    } else {
      ESP_LOGI(TAG, "Received packet is being skipt, not paired.");
    }
  } else {
    ESP_LOGE(TAG, "Invalid ESP-NOW packet received.");
    packet->options(OPTION_RAW, true);
    packet->status(TRIGGER_ON_RAW_DATA);
    this->get_raw_data_trigger()->trigger(packet);
  }
}

void ESPNowComponent::handle_system_command_(std::weak_ptr<ESPNowPacket> weak_packet, uint8_t command) {
  switch (command) {
    case ESPNOW_COMMAND_ACK:
    case ESPNOW_COMMAND_NACK:
      this->handle_ack_command_(weak_packet);
      break;
    case ESPNOW_COMMAND_RESEND:
      break;
    case ESPNOW_COMMAND_PING:
      break;
    case ESPNOW_COMMAND_PONG:
      break;
  }
}

void ESPNowComponent::handle_ack_command_(std::weak_ptr<ESPNowPacket> wAck) {
  auto ack = wAck.lock();
  uint64_t peer = ack->peer();
  uint64_t key = ack->read64u(0);
  bool status = ack->command() == ESPNOW_COMMAND_ACK;
  auto packet = this->get_packet(key);  // NOLINT
  bool unlock = true;

  if (packet == nullptr) {
    ESP_LOGE(TAG, "Packet %llx not found (ack).", key);
    return;
  }

  if (packet->peer() == ESPNOW_MULTICAST_ADDR) {
    packet->peer(peer);
    auto it = std::find(this->multicast_.begin(), this->multicast_.end(), peer);
    this->multicast_.erase(it);
    unlock = this->multicast_.empty();
  } else if (packet->peer() != ack->peer()) {
    ESP_LOGE(TAG, "%s| Invalid mac address. Got: %s", packet->info().c_str(), ack->info().c_str());
    return;
  } else if (status) {
    ESP_LOGV(TAG, "%s| Has been deliverd", packet->info().c_str());
  } else {
    ESP_LOGE(TAG, "%s| Failed to Send", packet->info().c_str());
  }

  packet->options(OPTION_FINISHED, true);

  if (status) {
    this->call_trigger_for_(TRIGGER_ON_SUCCEED, packet);
  } else {
    if (packet->options(OPTION_MULTICAST)) {
      this->call_trigger_for_(TRIGGER_ON_FAILED, packet);
    } else {
      packet->status(TRIGGER_ON_FAILED);
      packet->options(OPTION_BEEN_SEND, false);
    }
    unlock = false;
  }
}

bool ESPNowComponent::send(std::weak_ptr<ESPNowPacket> weak_packet) {
  auto packet = weak_packet.lock();
  if (packet == nullptr) {
    ESP_LOGE(TAG, "NO PACKET TO SEND !!!");
  } else if (!this->can_proceed()) {
    ESP_LOGW(TAG, "Network not ready, declining message send.");
  } else if (!this->is_ready()) {
    ESP_LOGE(TAG, "Cannot send espnow packet, espnow is not setup yet.");
  } else if (this->is_failed()) {
    ESP_LOGE(TAG, "Cannot send espnow packet, espnow failed to setup");
  } else if (this->send_queue_used() == SEND_BUFFER_SIZE) {
    ESP_LOGE(TAG, "Send Buffer Out of Memory.");
  } else if (this->is_suspension() && !packet->options(OPTION_SEND_DIRECT)) {
    ESP_LOGE(TAG, "Cannot send espnow packet, ESPNOW is suspended.");
  } else if (packet->peer() == this->own_peer_address_) {
    ESP_LOGE(TAG, "Trying to send a packet to your self.");
  } else if (!this->is_paired(packet->peer())) {
    ESP_LOGE(TAG, "Peer is not registered: %s.", packet->peer_str().c_str());
  } else if (!packet->is_valid() and !packet->options(OPTION_RAW)) {
    ESP_LOGE(TAG, "%s| Packet Is invalid !!", packet->info().c_str());
  } else {
    packet->sequents(this->get_next_sequents(packet->peer(), packet->application()));
    uint64_t key = packet->key();
    this->packet_send_map_[key] = packet;

    if (packet->options(OPTION_SEND_DIRECT)) {
      show_packet("Added to TOP of buffer", packet.get(), true);
      xQueueSendToFront(this->send_queue_, (void *) &key, 10);
    } else {
      show_packet("Added to BOTTOM of buffer", packet.get(), true);
      xQueueSendToBack(this->send_queue_, (void *) &key, 10);
    }

    return true;
  }
  return false;
}

bool ESPNowComponent::send_system_command(std::weak_ptr<ESPNowPacket> weak_packet, uint8_t command) {
  auto packet = weak_packet.lock();
  if (packet == nullptr || packet->options(OPTION_FINISHED)) {
    show_packet("Send System Command ALREADY SEND", packet.get());
    return false;
  }

  uint64_t key = make_key(this->own_peer_address_, packet->sequents());
  uint8_t *data = static_cast<uint8_t *>(static_cast<void *>(&key));

  auto ack = this->make_packet(packet->peer(), (const uint8_t *) data, sizeof(key), packet->application(), command);
  ack->options(OPTION_SEND_DIRECT, true);
  ack->options(OPTION_DONT_WAIT, true);
  packet->options(OPTION_FINISHED, true);
  return this->send(ack);
}

void ESPNowComponent::espnow_task(void *param) {
  ESPNowComponent *that = (ESPNowComponent *) param;
  uint64_t key{0}, key2{0};
  for (;;) {
    if (xQueuePeek(that->send_queue_, (void *) &key, (TickType_t) 1) == pdTRUE) {
      auto packet = that->get_packet(key);
      if (packet == nullptr) {
        ESP_LOGE(TAG, "Packet %llx not found (task).", key);
      } else if (packet->options(OPTION_FINISHED)) {
        xQueueReceive(that->send_queue_, (void *) &key2, (TickType_t) 1);
        if (key != key2) {
          ESP_LOGE(TAG, "!!! Remove from buffer ERROR: %llx vs %llx.", key, key2);
        }

      } else if (!packet->options(OPTION_BEEN_SEND)) {
        if (packet->attempt() >= that->attempts_) {
          ESP_LOGE(TAG, "%s| Packet Dropped. To many attempts. ", packet->info().c_str());
          xQueueReceive(that->send_queue_, (void *) &key2, (TickType_t) 1);
          if (key != key2) {
            ESP_LOGE(TAG, "!!! Remove from buffer ERROR: %llx vs %llx.", key, key2);
          }
          that->call_trigger_for_(packet->status(), packet);
        } else {
          packet->retry();
          packet->timestamp(millis());

          esp_err_t err;
          if (packet->peer() == ESPNOW_MULTICAST_ADDR) {
            that->start_multi_cast_();
            err = esp_now_send(nullptr, packet->content(), packet->content_size());
          } else {
            err = esp_now_send(packet->mac_address(), packet->content(), packet->content_size());
          }

          if (err == ESP_OK) {
            packet->options(OPTION_BEEN_SEND, true);
            if (packet->options(OPTION_DONT_WAIT)) {
              packet->options(OPTION_FINISHED, true);
              show_packet("Packet has been send. Dont Wait.", packet.get());

              that->call_trigger_for_(TRIGGER_ON_SUCCEED, packet);
            } else {
              show_packet("Packet has been send", packet.get());
            }
          } else {
            show_packet("FAILED to Send", packet.get());
            packet->status(TRIGGER_ON_FAILED);
          }
        }
      } else if (packet->timestamp() + that->confirmation_timeout_ < millis()) {
        show_packet("Timed Out", packet.get());
        packet->status(TRIGGER_ON_TIMEOUT);
        packet->options(OPTION_BEEN_SEND, false);

      } else {
        vTaskDelay(10);
      }
    }
  }
}

void ESPNowComponent::remove_packet(uint64_t key) {
  auto search = this->packet_send_map_.find(key);
  if (search != this->packet_send_map_.end()) {
    ESP_LOGW(TAG, "**  Removing Packet %llx, Count: %d", key, this->packet_send_map_[key].use_count());
    this->packet_send_map_[key] = nullptr;
    this->packet_send_map_.erase(key);
  } else {
    ESP_LOGE(TAG, "**  Removing Packet %llx FAILED.", key);
  }
}

esp_err_t ESPNowComponent::add_peer(uint64_t peer) {
  esp_err_t result = ESP_OK;
  esp_now_peer_info_t peer_info = {};

  if (this->is_ready()) {
    if (peer == this->own_peer_address_) {
      ESP_LOGE(TAG, "Tried to pair your self.");
      this->mark_failed();
      return ESP_ERR_INVALID_MAC;
    }
    if (!esp_now_is_peer_exist((uint8_t *) &peer)) {
      memset(&peer_info, 0, sizeof(esp_now_peer_info_t));
      peer_info.ifidx = WIFI_IF_STA;
      memcpy((void *) peer_info.peer_addr, (void *) &peer, 6);
      result = esp_now_add_peer(&peer_info);
    }
  }
  if (result == ESP_OK) {
    this->peers_.push_back(peer);
  }

  return result;
}

esp_err_t ESPNowComponent::del_peer(uint64_t peer) {
  esp_err_t result = ESP_OK;
  if (esp_now_is_peer_exist((uint8_t *) &peer)) {
    esp_err_t result = esp_now_del_peer((uint8_t *) &peer);
    if (result == ESP_OK) {
      this->call_on_del_peer_(peer);
    }
  }
  return result;
}

EPSNowTriggerCallback ESPNowComponent::get_trigger_for_(ESPNowTriggers event, std::weak_ptr<ESPNowPacket> weak_packet) {
  auto packet = weak_packet.lock();
  EPSNowTriggerCallback cb;
  if (packet->triggerGroup() != 0) {
    cb = this->triggers_[packet->triggerGroup()][packet->command()][event];
    if (!cb) {
      cb = this->triggers_[packet->triggerGroup()][0][event];
    }
  }

  if (!cb) {
    cb = this->triggers_[packet->application()][packet->command()][event];
  }
  if (!cb) {
    cb = this->triggers_[packet->application()][0][event];
  }

  if (!cb) {
    if (event == TRIGGER_ON_BROADCAST) {
      cb = this->get_trigger_for_(TRIGGER_ON_RECEIVE, weak_packet);
    }
    if (event == TRIGGER_ON_TIMEOUT) {
      cb = this->get_trigger_for_(TRIGGER_ON_FAILED, weak_packet);
    }
  }
  return cb;
}

void ESPNowComponent::call_trigger_for_(ESPNowTriggers event, std::weak_ptr<ESPNowPacket> weak_packet) {
  EPSNowTriggerCallback cb = this->get_trigger_for_(event, weak_packet);

  ESP_LOGI(TAG, "triggger event {%d}", (int) event);
  weak_packet.lock()->status(event);

  this->set_timeout(10, [this, cb, weak_packet]() {  //"ESPNowTrigger"
    auto packet = weak_packet.lock();
    show_packet("Running trigger ", packet.get());

    if (cb) {
      cb(packet);
    }

    if (packet->options(OPTION_RECEIVED)) {
      if (!packet->options(OPTION_FINISHED)) {
        this->send_system_command(packet, ESPNOW_COMMAND_ACK);
      }
    } else {
      if (packet->options(OPTION_FINISHED)) {
        uint64_t key = packet->key();
        this->remove_packet(key);
      }
    }
  });
}

void ESPNowComponent::call_on_add_peer_(uint64_t peer) {
  this->defer("Add_peer_trigger", [this, peer]() {
    for (const auto &kv : this->apps_) {
      kv.second->on_add_peer(peer);
    }
  });
}

void ESPNowComponent::call_on_del_peer_(uint64_t peer) {
  this->defer("del_peer_trigger", [this, peer]() {
    for (const auto &kv : this->apps_) {
      kv.second->on_del_peer(peer);
    }
  });
}

bool ESPNowComponent::is_paired(uint64_t peer) {
  bool result = false;
  if (peer == ESPNOW_MULTICAST_ADDR) {
    return true;
  } else if (peer == ESPNOW_BROADCAST_ADDR) {
    this->add_peer(ESPNOW_BROADCAST_ADDR);
    return true;
  } else if (this->pairing_app_ != nullptr) {
    result = this->pairing_app_->is_paired(peer);
  } else {
    result = (esp_now_is_peer_exist((uint8_t *) &peer));
  }
  if (!result && this->auto_add_peer_) {
    result = (this->add_peer(peer) == ESP_OK);
  }
  return result;
}

void ESPNowComponent::start_multi_cast_() {
  esp_now_peer_info_t peer;
  memset(&peer, 0, sizeof(esp_now_peer_info_t));
  multicast_.clear();
  for (esp_err_t e = esp_now_fetch_peer(true, &peer); e == ESP_OK; e = esp_now_fetch_peer(false, &peer)) {
    uint64_t mac = 0;
    memcpy((void *) &mac, peer.peer_addr, 6);
    multicast_.push_back(mac);
  }
}

uint16_t ESPNowComponent::get_next_sequents(uint64_t peer, uint16_t app_id) {
  if (this->next_sequents_[peer][app_id] == 0xFFFF) {
    this->next_sequents_[peer][app_id] = 0;
  } else {
    this->next_sequents_[peer][app_id]++;
  }
  return this->next_sequents_[peer][app_id];
}

bool ESPNowComponent::is_valid_squence(uint64_t peer, uint16_t app_id, uint16_t received_sequence) {
  bool valid = this->next_sequents_[peer][app_id] + 1 == received_sequence;
  if (valid) {
    this->next_sequents_[peer][app_id] = received_sequence;
  }
  return valid;
}

/* ESPNowApp ********************************************************************** */

bool ESPNowApp::send(uint64_t peer, const uint8_t *data, uint8_t len, uint8_t command) {
  if (peer == 0ULL)
    return false;

  auto packet = this->parent_->make_packet(peer, data, len, this->application(), command);  // NOLINT

  return this->parent_->send(packet);
}

}  // namespace espnow
}  // namespace esphome

#endif
#endif
