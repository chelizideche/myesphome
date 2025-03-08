#include "esphome/core/defines.h"
#include "openthread.h"

#if defined(USE_ZEPHYR) && defined(USE_OPENTHREAD_ZEPHYR)

#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/application.h"

#include <zephyr/kernel.h>
#include <zephyr/net/openthread.h>
#include <openthread/thread.h>
#include <openthread/ip6.h>
#include <openthread/dataset.h>
#include <openthread/platform/radio.h>
#include <openthread/srp_client.h>
#include <openthread/dataset_ftd.h>

namespace esphome {
namespace openthread_zephyr {

static const char *const TAG = "openthread_zephyr";

// Add a global variable for the OpenThreadZephyr component
OpenThreadZephyr *global_openthread_zephyr = nullptr;

void OpenThreadZephyr::setup() {
  ESP_LOGCONFIG(TAG, "Setting up OpenThread Zephyr...");
  
  // Set the global variable
  global_openthread_zephyr = this;
  
  // Initialize OpenThread stack
  int ret = openthread_start(openthread_get_default_context());
  if (ret != 0) {
    ESP_LOGE(TAG, "Failed to initialize OpenThread stack: %d", ret);
    this->mark_failed();
    return;
  }
  
  // Configure operational dataset if needed
  if (this->force_dataset_) {
    this->configure_operational_dataset();
  }
  
  // Start OpenThread network
  this->start_thread_network();
  
  // Enable SRP client auto-start
  otInstance *instance = openthread_get_default_instance();
  if (instance != nullptr) {
    otSrpClientEnableAutoStartMode(instance, nullptr, nullptr);
    ESP_LOGD(TAG, "SRP client auto-start enabled");
  }
}

void OpenThreadZephyr::loop() {
  if (!this->thread_started_)
    return;
    
  // Check connection status
  otInstance *instance = openthread_get_default_instance();
  if (instance == nullptr) {
    ESP_LOGW(TAG, "OpenThread instance not available");
    return;
  }
  
  uint8_t role = otThreadGetDeviceRole(instance);
  bool is_connected = role >= OT_DEVICE_ROLE_CHILD;
  
  if (role != this->thread_role_) {
    this->thread_role_ = role;
    const char *role_str = "Unknown";
    switch (role) {
      case OT_DEVICE_ROLE_DISABLED:
        role_str = "Disabled";
        break;
      case OT_DEVICE_ROLE_DETACHED:
        role_str = "Detached";
        break;
      case OT_DEVICE_ROLE_CHILD:
        role_str = "Child";
        break;
      case OT_DEVICE_ROLE_ROUTER:
        role_str = "Router";
        break;
      case OT_DEVICE_ROLE_LEADER:
        role_str = "Leader";
        break;
    }
    ESP_LOGI(TAG, "Thread role changed to: %s", role_str);
  }
  
  if (is_connected != this->connected_) {
    this->connected_ = is_connected;
    if (this->connected_) {
      ESP_LOGI(TAG, "Connected to Thread network");
      
      // Update IPv6 addresses
      this->update_ipv6_addresses();
    } else {
      ESP_LOGW(TAG, "Disconnected from Thread network");
      this->has_ipv6_address_ = false;
      this->ipv6_address_ = "";
    }
  }
  
  // Periodically update IPv6 addresses when connected
  static uint32_t last_ipv6_update = 0;
  if (this->connected_ && (millis() - last_ipv6_update > 30000)) {  // Every 30 seconds
    this->update_ipv6_addresses();
    last_ipv6_update = millis();
  }
}

void OpenThreadZephyr::dump_config() {
  ESP_LOGCONFIG(TAG, "OpenThread Zephyr:");
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);
  ESP_LOGCONFIG(TAG, "  PAN ID: 0x%04X", this->panid_);
  ESP_LOGCONFIG(TAG, "  Network Name: %s", this->network_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Extended PAN ID: %s", this->xpanid_.c_str());
  ESP_LOGCONFIG(TAG, "  Radio TX Power: %d dBm", this->radio_tx_power_);
  ESP_LOGCONFIG(TAG, "  Force Dataset: %s", YESNO(this->force_dataset_));
  ESP_LOGCONFIG(TAG, "  Connected: %s", YESNO(this->connected_));
  
  if (this->has_ipv6_address_) {
    ESP_LOGCONFIG(TAG, "  IPv6 Address: %s", this->ipv6_address_.c_str());
  }
}

void OpenThreadZephyr::configure_operational_dataset() {
  otInstance *instance = openthread_get_default_instance();
  if (instance == nullptr) {
    ESP_LOGE(TAG, "OpenThread instance not available");
    return;
  }
  
  otOperationalDataset dataset;
  memset(&dataset, 0, sizeof(dataset));
  
  // Set active timestamp
  dataset.mActiveTimestamp.mSeconds = 1;
  dataset.mActiveTimestamp.mTicks = 0;
  dataset.mActiveTimestamp.mAuthoritative = false;
  dataset.mComponents.mIsActiveTimestampPresent = true;
  
  // Set channel
  dataset.mChannel = this->channel_;
  dataset.mComponents.mIsChannelPresent = true;
  
  // Set PAN ID
  dataset.mPanId = this->panid_;
  dataset.mComponents.mIsPanIdPresent = true;
  
  // Set Extended PAN ID - Fixed implementation
  const char *xpanid_str = this->xpanid_.c_str();
  for (size_t i = 0; i < sizeof(dataset.mExtendedPanId.m8); i++) {
    char hex[3] = {xpanid_str[i * 2], xpanid_str[i * 2 + 1], 0};
    dataset.mExtendedPanId.m8[i] = strtoul(hex, nullptr, 16);
  }
  dataset.mComponents.mIsExtendedPanIdPresent = true;
  
  // Set network key - Fixed implementation
  const char *key_str = this->network_key_.c_str();
  for (size_t i = 0; i < sizeof(dataset.mNetworkKey.m8); i++) {
    char hex[3] = {key_str[i * 2], key_str[i * 2 + 1], 0};
    dataset.mNetworkKey.m8[i] = strtoul(hex, nullptr, 16);
  }
  dataset.mComponents.mIsNetworkKeyPresent = true;
  
  // Set network name
  strncpy(dataset.mNetworkName.m8, this->network_name_.c_str(), sizeof(dataset.mNetworkName.m8) - 1);
  dataset.mComponents.mIsNetworkNamePresent = true;
  
  // Set PSKC - Fixed implementation
  const char *pskc_str = this->pskc_.c_str();
  for (size_t i = 0; i < sizeof(dataset.mPskc.m8); i++) {
    char hex[3] = {pskc_str[i * 2], pskc_str[i * 2 + 1], 0};
    dataset.mPskc.m8[i] = strtoul(hex, nullptr, 16);
  }
  dataset.mComponents.mIsPskcPresent = true;
  
  // Set the operational dataset
  otError error = otDatasetSetActive(instance, &dataset);
  if (error != OT_ERROR_NONE) {
    ESP_LOGE(TAG, "Failed to set operational dataset: %d", error);
  } else {
    ESP_LOGI(TAG, "Successfully set operational dataset");
  }
}

void OpenThreadZephyr::start_thread_network() {
  otInstance *instance = openthread_get_default_instance();
  if (instance == nullptr) {
    ESP_LOGE(TAG, "OpenThread instance not available");
    return;
  }
  
  if (!this->force_dataset_) {
    // Set extended PAN ID - Fixed implementation
    otExtendedPanId xpanid;
    const char *xpanid_str = this->xpanid_.c_str();
    for (size_t i = 0; i < sizeof(xpanid.m8); i++) {
      char hex[3] = {xpanid_str[i * 2], xpanid_str[i * 2 + 1], 0};
      xpanid.m8[i] = strtoul(hex, nullptr, 16);
    }
    otError error = otThreadSetExtendedPanId(instance, &xpanid);
    if (error != OT_ERROR_NONE) {
      ESP_LOGE(TAG, "Failed to set extended PAN ID: %d", error);
    }
    
    // Set network key - Fixed implementation
    otNetworkKey network_key;
    const char *key_str = this->network_key_.c_str();
    for (size_t i = 0; i < sizeof(network_key.m8); i++) {
      char hex[3] = {key_str[i * 2], key_str[i * 2 + 1], 0};
      network_key.m8[i] = strtoul(hex, nullptr, 16);
    }
    error = otThreadSetNetworkKey(instance, &network_key);
    if (error != OT_ERROR_NONE) {
      ESP_LOGE(TAG, "Failed to set network key: %d", error);
    }
    
    // Set channel
    error = otLinkSetChannel(instance, this->channel_);
    if (error != OT_ERROR_NONE) {
      ESP_LOGE(TAG, "Failed to set channel: %d", error);
    }
    
    // Set PAN ID
    error = otLinkSetPanId(instance, this->panid_);
    if (error != OT_ERROR_NONE) {
      ESP_LOGE(TAG, "Failed to set PAN ID: %d", error);
    }
    
    // Set network name
    error = otThreadSetNetworkName(instance, this->network_name_.c_str());
    if (error != OT_ERROR_NONE) {
      ESP_LOGE(TAG, "Failed to set network name: %d", error);
    }
  }
  
  // Set radio TX power
  otPlatRadioSetTransmitPower(instance, this->radio_tx_power_);
  
  // Enable Thread
  otError error = otThreadSetEnabled(instance, true);
  if (error != OT_ERROR_NONE) {
    ESP_LOGE(TAG, "Failed to enable Thread: %d", error);
    return;
  }
  
  ESP_LOGI(TAG, "Started Thread network");
  this->thread_started_ = true;
}

void OpenThreadZephyr::stop_thread_network() {
  otInstance *instance = openthread_get_default_instance();
  if (instance == nullptr) {
    ESP_LOGE(TAG, "OpenThread instance not available");
    return;
  }
  
  otError error = otThreadSetEnabled(instance, false);
  if (error != OT_ERROR_NONE) {
    ESP_LOGE(TAG, "Failed to disable Thread: %d", error);
    return;
  }
  
  ESP_LOGI(TAG, "Stopped Thread network");
  this->thread_started_ = false;
  this->connected_ = false;
  this->has_ipv6_address_ = false;
  this->ipv6_address_ = "";
}

void OpenThreadZephyr::update_ipv6_addresses() {
  otInstance *instance = openthread_get_default_instance();
  if (instance == nullptr) {
    ESP_LOGW(TAG, "OpenThread instance not available");
    return;
  }
  
  // Get unicast addresses
  const otNetifAddress *unicast_addrs = otIp6GetUnicastAddresses(instance);
  bool found_mesh_local = false;
  bool found_global = false;
  std::string mesh_local_addr;
  std::string global_addr;
  
  for (const otNetifAddress *addr = unicast_addrs; addr; addr = addr->mNext) {
    char addr_str[OT_IP6_ADDRESS_STRING_SIZE];
    otIp6AddressToString(&addr->mAddress, addr_str, sizeof(addr_str));
    
    // Check if mesh-local address
    if (addr->mAddress.mFields.m8[0] == 0xfd) {
      found_mesh_local = true;
      mesh_local_addr = addr_str;
      ESP_LOGD(TAG, "Mesh-local IPv6 Address: %s", addr_str);
    }
    // Check if global address
    else if (addr->mAddress.mFields.m8[0] == 0x20 || addr->mAddress.mFields.m8[0] == 0x30) {
      found_global = true;
      global_addr = addr_str;
      ESP_LOGD(TAG, "Global IPv6 Address: %s", addr_str);
    }
  }
  
  // Update IPv6 address status
  bool had_ipv6 = this->has_ipv6_address_;
  this->has_ipv6_address_ = found_mesh_local || found_global;
  
  // Prefer global address, fall back to mesh-local
  if (found_global) {
    this->ipv6_address_ = global_addr;
  } else if (found_mesh_local) {
    this->ipv6_address_ = mesh_local_addr;
  }
  
  // Log if IPv6 status changed
  if (had_ipv6 != this->has_ipv6_address_) {
    if (this->has_ipv6_address_) {
      ESP_LOGI(TAG, "IPv6 address acquired: %s", this->ipv6_address_.c_str());
    } else {
      ESP_LOGW(TAG, "IPv6 address lost");
    }
  }
}

}  // namespace openthread_zephyr
}  // namespace esphome

#endif  // defined(USE_ZEPHYR) && defined(USE_OPENTHREAD_ZEPHYR) 