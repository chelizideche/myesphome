#ifdef USE_ZEPHYR

#include "mdns.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <zephyr/net/openthread.h>
#include <openthread/srp_client.h>
#include <openthread/thread.h>

namespace esphome {
namespace openthread_zephyr {

static const char *const TAG = "openthread_zephyr.mdns";

void OpenThreadMDNSBridge::setup() {
  ESP_LOGCONFIG(TAG, "Setting up OpenThread mDNS bridge...");
  
  // Get hostname from App
  this->hostname_ = App.get_name();
  
  // Initial sync will happen in loop() once OpenThread is connected
}

void OpenThreadMDNSBridge::loop() {
  // Check if we need to sync services
  if (!this->services_registered_ && this->mdns_ != nullptr) {
    this->sync_services_();
  }
}

void OpenThreadMDNSBridge::dump_config() {
  ESP_LOGCONFIG(TAG, "OpenThread mDNS Bridge:");
  ESP_LOGCONFIG(TAG, "  Hostname: %s", this->hostname_.c_str());
}

void OpenThreadMDNSBridge::sync_services_() {
  otInstance *instance = openthread_get_default_instance();
  if (instance == nullptr) {
    ESP_LOGW(TAG, "OpenThread instance not available, will retry later");
    return;
  }
  
  // Check if OpenThread is connected
  if (otThreadGetDeviceRole(instance) < OT_DEVICE_ROLE_CHILD) {
    ESP_LOGV(TAG, "OpenThread not connected yet, will sync services later");
    return;
  }
  
  // Set the host name for SRP
  otError error = otSrpClientSetHostName(instance, this->hostname_.c_str());
  if (error != OT_ERROR_NONE) {
    ESP_LOGE(TAG, "Failed to set SRP host name: %d", error);
    return;
  }
  
  // Enable auto host address
  error = otSrpClientEnableAutoHostAddress(instance);
  if (error != OT_ERROR_NONE) {
    ESP_LOGE(TAG, "Failed to enable SRP client auto host address: %d", error);
    return;
  }
  
  // Get services from mDNS component
  for (const auto &service : this->mdns_->services_) {
    std::string service_type = service.service_type;
    std::string proto = service.proto;
    uint16_t port = service.port;
    
    // Convert TXT records from vector to map
    std::map<std::string, std::string> txt_records;
    for (const auto &record : service.txt_records) {
      txt_records[record.key] = record.value;
    }
    
    // Register service with OpenThread SRP
    this->register_service(service_type, proto, port, txt_records);
  }
  
  this->services_registered_ = true;
  ESP_LOGI(TAG, "Synced mDNS services to OpenThread SRP");
}

bool OpenThreadMDNSBridge::register_service(const std::string &service_type, const std::string &proto, uint16_t port,
                                          const std::map<std::string, std::string> &txt_records) {
  otInstance *instance = openthread_get_default_instance();
  if (instance == nullptr) {
    ESP_LOGE(TAG, "OpenThread instance not available");
    return false;
  }
  
  // Format service type
  std::string full_service_type = service_type;
  if (!proto.empty()) {
    full_service_type += "." + proto;
  }
  
  // Prepare TXT records
  uint8_t txt_buffer[256];
  size_t txt_len = 0;
  
  for (const auto &record : txt_records) {
    std::string entry = record.first + "=" + record.second;
    size_t entry_len = entry.length();
    
    // Check if we have enough space
    if (txt_len + entry_len + 1 > sizeof(txt_buffer)) {
      ESP_LOGW(TAG, "TXT record buffer full, skipping record: %s", entry.c_str());
      continue;
    }
    
    // Add length byte
    txt_buffer[txt_len++] = entry_len;
    
    // Add entry data
    memcpy(txt_buffer + txt_len, entry.c_str(), entry_len);
    txt_len += entry_len;
  }
  
  // Create a service structure
  otSrpClientService service;
  memset(&service, 0, sizeof(service));
  
  service.mName = full_service_type.c_str();
  service.mInstanceName = this->hostname_.c_str();
  service.mPort = port;
  
  // Add the service
  otError error = otSrpClientAddService(instance, &service);
  if (error != OT_ERROR_NONE) {
    ESP_LOGE(TAG, "Failed to add SRP service: %d", error);
    return false;
  }
  
  ESP_LOGI(TAG, "Registered service: %s on port %d", full_service_type.c_str(), port);
  return true;
}

}  // namespace openthread_zephyr
}  // namespace esphome

#endif  // USE_ZEPHYR 