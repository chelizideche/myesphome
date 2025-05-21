
#include "wifi_component.h"

#ifdef USE_WIFI
#ifdef USE_RP2040

#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/netif.h"
#include <AddrList.h>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

namespace esphome {
namespace wifi {

static const char *const TAG = "wifi_pico_w";

#ifdef USE_RP2040_ESPHOST
static bool s_sta_connecting = false;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
#endif

bool WiFiComponent::wifi_mode_(optional<bool> sta, optional<bool> ap) {
#ifndef USE_RP2040_ESPHOST
  if (sta.has_value()) {
    if (sta.value()) {
      cyw43_wifi_set_up(&cyw43_state, CYW43_ITF_STA, true, CYW43_COUNTRY_WORLDWIDE);
    }
  }
  if (ap.has_value()) {
    if (ap.value()) {
      cyw43_wifi_set_up(&cyw43_state, CYW43_ITF_AP, true, CYW43_COUNTRY_WORLDWIDE);
    }
  }
  return true;
#else
  uint8_t current_mode = WiFi.getMode();
  bool current_sta = current_mode & WIFI_STA;
  bool current_ap = current_mode & WIFI_AP;
  bool enable_sta = sta.value_or(current_sta);
  bool enable_ap = ap.value_or(current_ap);
  if (current_sta == enable_sta && current_ap == enable_ap)
    return true;

  uint8_t mode = 0;
  if (enable_sta)
    mode |= WIFI_STA;
  if (enable_ap)
    mode |= WIFI_AP;
  WiFi.mode(static_cast<WiFiMode_t>(mode));
  return true;
#endif
}

bool WiFiComponent::wifi_apply_power_save_() {
#ifndef USE_RP2040_ESPHOST
  uint32_t pm;
  switch (this->power_save_) {
    case WIFI_POWER_SAVE_NONE:
      pm = CYW43_PERFORMANCE_PM;
      break;
    case WIFI_POWER_SAVE_LIGHT:
      pm = CYW43_DEFAULT_PM;
      break;
    case WIFI_POWER_SAVE_HIGH:
      pm = CYW43_AGGRESSIVE_PM;
      break;
  }
  int ret = cyw43_wifi_pm(&cyw43_state, pm);
  return ret == 0;
#else
  return false;
#endif
}

// TODO: The driver doesnt seem to have an API for this
bool WiFiComponent::wifi_apply_output_power_(float output_power) { return true; }

bool WiFiComponent::wifi_sta_connect_(const WiFiAP &ap) {
  if (!this->wifi_sta_ip_config_(ap.get_manual_ip()))
    return false;

  auto ret = WiFi.begin(ap.get_ssid().c_str(), ap.get_password().c_str());
  if (ret != WL_CONNECTED)
    return false;

  return true;
}

bool WiFiComponent::wifi_sta_pre_setup_() { return this->wifi_mode_(true, {}); }

bool WiFiComponent::wifi_sta_ip_config_(optional<ManualIP> manual_ip) {
  if (!manual_ip.has_value()) {
    return true;
  }

  IPAddress ip_address = manual_ip->static_ip;
  IPAddress gateway = manual_ip->gateway;
  IPAddress subnet = manual_ip->subnet;

  IPAddress dns = manual_ip->dns1;

  WiFi.config(ip_address, dns, gateway, subnet);
  return true;
}

bool WiFiComponent::wifi_apply_hostname_() {
  WiFi.setHostname(App.get_name().c_str());
  return true;
}
const char *get_auth_mode_str(uint8_t mode) {
  // TODO:
  return "UNKNOWN";
}
const char *get_disconnect_reason_str(uint8_t reason) {
  // TODO:
  return "UNKNOWN";
}

WiFiSTAConnectStatus WiFiComponent::wifi_sta_connect_status_() {
#ifndef USE_RP2040_ESPHOST
  int status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
  switch (status) {
    case CYW43_LINK_JOIN:
    case CYW43_LINK_NOIP:
      return WiFiSTAConnectStatus::CONNECTING;
    case CYW43_LINK_UP:
      return WiFiSTAConnectStatus::CONNECTED;
    case CYW43_LINK_FAIL:
    case CYW43_LINK_BADAUTH:
      return WiFiSTAConnectStatus::ERROR_CONNECT_FAILED;
    case CYW43_LINK_NONET:
      return WiFiSTAConnectStatus::ERROR_NETWORK_NOT_FOUND;
  }
  return WiFiSTAConnectStatus::IDLE;
#else
  auto status = WiFi.status();
  if (status == WL_CONNECTED) {
    return WiFiSTAConnectStatus::CONNECTED;
  } else if (status == WL_CONNECT_FAILED || status == WL_CONNECTION_LOST) {
    return WiFiSTAConnectStatus::ERROR_CONNECT_FAILED;
  } else if (status == WL_NO_SSID_AVAIL) {
    return WiFiSTAConnectStatus::ERROR_NETWORK_NOT_FOUND;
  } else if (s_sta_connecting) {
    return WiFiSTAConnectStatus::CONNECTING;
  }
  return WiFiSTAConnectStatus::IDLE;
#endif
}

#ifndef USE_RP2040_ESPHOST
int WiFiComponent::s_wifi_scan_result(void *env, const cyw43_ev_scan_result_t *result) {
  global_wifi_component->wifi_scan_result(env, result);
  return 0;
}

void WiFiComponent::wifi_scan_result(void *env, const cyw43_ev_scan_result_t *result) {
  bssid_t bssid;
  std::copy(result->bssid, result->bssid + 6, bssid.begin());
  std::string ssid(reinterpret_cast<const char *>(result->ssid));
  WiFiScanResult res(bssid, ssid, result->channel, result->rssi, result->auth_mode != CYW43_AUTH_OPEN, ssid.empty());
  if (std::find(this->scan_result_.begin(), this->scan_result_.end(), res) == this->scan_result_.end()) {
    this->scan_result_.push_back(res);
  }
}

bool WiFiComponent::wifi_scan_start_(bool passive) {
  this->scan_result_.clear();
  this->scan_done_ = false;
  cyw43_wifi_scan_options_t scan_options = {0};
  scan_options.scan_type = passive ? 1 : 0;
  int err = cyw43_wifi_scan(&cyw43_state, &scan_options, nullptr, &s_wifi_scan_result);
  if (err) {
    ESP_LOGV(TAG, "cyw43_wifi_scan failed!");
  }
  return err == 0;
  return true;
}
#else
bool WiFiComponent::wifi_scan_start_(bool passive) {
  this->scan_result_.clear();

  int16_t num = WiFi.scanNetworks(false);
  if (num < 0)
    return false;

  this->scan_result_.reserve(static_cast<unsigned int>(num));
  for (int i = 0; i < num; i++) {
    String ssid = WiFi.SSID(i);
    uint8_t authmode = WiFi.encryptionType(i);
    int32_t rssi = WiFi.RSSI(i);
    uint8_t bssid[6];
    WiFi.BSSID(i, bssid);
    int32_t channel = WiFi.channel(i);

    WiFiScanResult scan({bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]}, std::string(ssid.c_str()),
                        channel, rssi, authmode != WIFI_AUTH_OPEN, ssid.length() == 0);
    this->scan_result_.push_back(scan);
  }
  WiFi.scanDelete();
  this->scan_done_ = true;
  return true;
}
#endif

#ifdef USE_WIFI_AP
bool WiFiComponent::wifi_ap_ip_config_(optional<ManualIP> manual_ip) {
  esphome::network::IPAddress ip_address, gateway, subnet, dns;
  if (manual_ip.has_value()) {
    ip_address = manual_ip->static_ip;
    gateway = manual_ip->gateway;
    subnet = manual_ip->subnet;
    dns = manual_ip->static_ip;
  } else {
    ip_address = network::IPAddress(192, 168, 4, 1);
    gateway = network::IPAddress(192, 168, 4, 1);
    subnet = network::IPAddress(255, 255, 255, 0);
    dns = network::IPAddress(192, 168, 4, 1);
  }
  WiFi.config(ip_address, dns, gateway, subnet);
  return true;
}

bool WiFiComponent::wifi_start_ap_(const WiFiAP &ap) {
  if (!this->wifi_mode_({}, true))
    return false;
  if (!this->wifi_ap_ip_config_(ap.get_manual_ip())) {
    ESP_LOGV(TAG, "wifi_ap_ip_config_ failed!");
    return false;
  }

  WiFi.beginAP(ap.get_ssid().c_str(), ap.get_password().c_str(), ap.get_channel().value_or(1));

  return true;
}

network::IPAddress WiFiComponent::wifi_soft_ap_ip() { return {(const ip_addr_t *) WiFi.localIP()}; }
#endif  // USE_WIFI_AP

bool WiFiComponent::wifi_disconnect_() {
#ifndef USE_RP2040_ESPHOST
  int err = cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
  return err == 0;
#else
  return WiFi.disconnect() == WL_DISCONNECTED;
#endif
}

bssid_t WiFiComponent::wifi_bssid() {
  bssid_t bssid{};
  uint8_t raw_bssid[6];
  WiFi.BSSID(raw_bssid);
  for (size_t i = 0; i < bssid.size(); i++)
    bssid[i] = raw_bssid[i];
  return bssid;
}
std::string WiFiComponent::wifi_ssid() { return WiFi.SSID().c_str(); }
int8_t WiFiComponent::wifi_rssi() { return WiFi.RSSI(); }
int32_t WiFiComponent::get_wifi_channel() { return WiFi.channel(); }

network::IPAddresses WiFiComponent::wifi_sta_ip_addresses() {
  network::IPAddresses addresses;
  uint8_t index = 0;
  for (auto addr : addrList) {
    addresses[index++] = addr.ipFromNetifNum();
  }
  return addresses;
}
network::IPAddress WiFiComponent::wifi_subnet_mask_() { return {(const ip_addr_t *) WiFi.subnetMask()}; }
network::IPAddress WiFiComponent::wifi_gateway_ip_() { return {(const ip_addr_t *) WiFi.gatewayIP()}; }
network::IPAddress WiFiComponent::wifi_dns_ip_(int num) {
  const ip_addr_t *dns_ip = dns_getserver(num);
  return network::IPAddress(dns_ip);
}

void WiFiComponent::wifi_loop_() {
#ifndef USE_RP2040_ESPHOST
  if (this->state_ == WIFI_COMPONENT_STATE_STA_SCANNING && !cyw43_wifi_scan_active(&cyw43_state)) {
    this->scan_done_ = true;
    ESP_LOGV(TAG, "Scan done!");
  }
#endif
}

void WiFiComponent::wifi_pre_setup_() {}

}  // namespace wifi
}  // namespace esphome

#endif
#endif
