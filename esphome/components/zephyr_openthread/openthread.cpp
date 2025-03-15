#include "esphome/core/defines.h"
#include "openthread.h"

#if defined(USE_ZEPHYR) && defined(USE_ZEPHYR_OPENTHREAD)

#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/application.h"

#include <zephyr/kernel.h>
#include <zephyr/net/openthread.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_l2.h>
#include <openthread/thread.h>
#include <openthread/ip6.h>
#include <openthread/dataset.h>
#include <openthread/platform/radio.h>
#include <openthread/srp_client.h>
#include <openthread/srp_client_buffers.h>
#include <openthread/dataset_ftd.h>
#include <openthread/netdata.h>
#include "esphome/components/socket/socket.h"

namespace esphome {
namespace zephyr_openthread {

static const char *const TAG = "zephyr_openthread";

// Add a global variable for the OpenThreadZephyr component
OpenThreadZephyr *global_openthread_component = nullptr;

// SRP client callbacks
void srpCallback(otError aError, const otSrpClientHostInfo *aHostInfo, const otSrpClientService *aServices,
                 const otSrpClientService *aRemovedServices, void *aContext) {
  if (aError == OT_ERROR_NONE) {
    ESP_LOGI(TAG, "SRP client registration successful");
  } else {
    ESP_LOGW(TAG, "SRP client registration failed: %s", otThreadErrorToString(aError));
  }
}

void srpStartCallback(const otSockAddr *aServerSockAddr, void *aContext) { ESP_LOGI(TAG, "SRP client has started"); }

void OpenThreadZephyr::setup() {
  ESP_LOGCONFIG(TAG, "Setting up OpenThread Zephyr...");

  // Set the global variable
  global_openthread_component = this;

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
}

void OpenThreadZephyr::loop() {
  // Add debug logging
  static uint32_t last_debug_log = 0;
  if (millis() - last_debug_log > 5000) {  // Log every 5 seconds
    last_debug_log = millis();
  }

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

      // Note: SRP entries will be automatically updated when Off-Mesh-Routable prefix changes
      // due to otSrpClientEnableAutoHostAddress being enabled in setup_srp_services()
    } else {
      ESP_LOGW(TAG, "Disconnected from Thread network");
      this->has_ipv6_address_ = false;
      this->ipv6_address_ = "";

      // Reset SRP services registration flag when disconnected
      this->srp_services_registered_ = false;
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
  ESP_LOGD(TAG, "Updating IPv6 addresses...");

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
  bool found_omr_prefix = false;

  ESP_LOGD(TAG, "Scanning IPv6 addresses:");
  int addr_count = 0;

  for (const otNetifAddress *addr = unicast_addrs; addr; addr = addr->mNext) {
    addr_count++;
    char addr_str[OT_IP6_ADDRESS_STRING_SIZE];
    otIp6AddressToString(&addr->mAddress, addr_str, sizeof(addr_str));

    // Log all addresses
    ESP_LOGD(TAG, "  Address %d: %s/%d", addr_count, addr_str, addr->mPrefixLength);

    // Check if mesh-local address (fd prefix)
    if (addr->mAddress.mFields.m8[0] == 0xfd) {
      found_mesh_local = true;
      mesh_local_addr = addr_str;
      ESP_LOGD(TAG, "  -> Mesh-local IPv6 Address: %s", addr_str);
    }
    // Check if global address (2xxx or 3xxx prefix)
    else if ((addr->mAddress.mFields.m8[0] == 0x20 || addr->mAddress.mFields.m8[0] == 0x30) &&
             addr->mPrefixLength == 64) {
      found_global = true;
      global_addr = addr_str;
      ESP_LOGD(TAG, "  -> Global IPv6 Address: %s", addr_str);
      found_omr_prefix = true;
      ESP_LOGD(TAG, "  -> Off-Mesh-Routable (OMR) prefix detected: %s/%d", addr_str, addr->mPrefixLength);
    }
    // Check if link-local address (fe80 prefix)
    else if (addr->mAddress.mFields.m8[0] == 0xfe && addr->mAddress.mFields.m8[1] == 0x80) {
      ESP_LOGD(TAG, "  -> Link-local IPv6 Address: %s", addr_str);
      // If we don't have a better address, use link-local as fallback
      if (!found_mesh_local && !found_global) {
        mesh_local_addr = addr_str;
      }
    }
  }

  if (addr_count == 0) {
    ESP_LOGD(TAG, "  No IPv6 addresses found");
  }

  // Update IPv6 address status
  bool had_ipv6 = this->has_ipv6_address_;
  this->has_ipv6_address_ = found_mesh_local || found_global;

  // Prefer global address, fall back to mesh-local
  if (found_global) {
    this->ipv6_address_ = global_addr;
    ESP_LOGI(TAG, "Using global IPv6 address: %s", global_addr.c_str());
  } else if (found_mesh_local) {
    this->ipv6_address_ = mesh_local_addr;
    ESP_LOGI(TAG, "Using mesh-local IPv6 address: %s", mesh_local_addr.c_str());
  }

  // Log if IPv6 status changed
  if (had_ipv6 != this->has_ipv6_address_) {
    if (this->has_ipv6_address_) {
      ESP_LOGI(TAG, "IPv6 address acquired: %s", this->ipv6_address_.c_str());

      ESP_LOGD(TAG, "Thread network interface initialized");

      // Setup SRP services if not already done
      if (!this->srp_services_registered_ && this->mdns_ != nullptr) {
        ESP_LOGI(TAG, "Setting up SRP services now that we have an IP address");
        this->setup_srp_services();
        this->srp_services_registered_ = true;
      }

      // If we have an OMR prefix, SRP auto host address will handle updating SRP entries
      if (found_omr_prefix) {
        ESP_LOGD(TAG, "OMR prefix available - SRP entries will be updated automatically");
      }
    } else {
      ESP_LOGW(TAG, "IPv6 address lost");
    }
  }
}

void *OpenThreadZephyr::pool_alloc_(size_t size) {
  uint8_t *ptr = new uint8_t[size];
  this->memory_pool_.emplace_back(std::unique_ptr<uint8_t[]>(ptr));
  return ptr;
}

void OpenThreadZephyr::setup_srp_services() {
  otInstance *instance = openthread_get_default_instance();
  if (instance == nullptr) {
    ESP_LOGE(TAG, "OpenThread instance not available for SRP setup");
    return;
  }

  otSrpClientSetCallback(instance, srpCallback, nullptr);

  // Set the host name using the project name
  std::string hostname_str = App.get_name();  // Store in a string to ensure lifetime
  this->host_name_ = hostname_str;            // Store for later use

  // Get buffer for hostname
  uint16_t size;
  char *existing_host_name = otSrpClientBuffersGetHostNameString(instance, &size);
  uint16_t len = this->host_name_.size();
  if (len > size) {
    ESP_LOGW(TAG, "Hostname is too long, choose a shorter project name");
    return;
  }
  memcpy(existing_host_name, this->host_name_.c_str(), len + 1);

  // Set the hostname
  otError error = otSrpClientSetHostName(instance, existing_host_name);
  if (error != OT_ERROR_NONE) {
    ESP_LOGW(TAG, "Could not set host name: %s", otThreadErrorToString(error));
    return;
  }

  // Enable automatic host address updates when Off-Mesh-Routable prefix changes
  error = otSrpClientEnableAutoHostAddress(instance);
  if (error != OT_ERROR_NONE) {
    ESP_LOGW(TAG, "Could not enable auto host address: %s", otThreadErrorToString(error));
    return;
  }

  // If mDNS component is available, use its services
  if (this->mdns_ != nullptr) {
    // Copy the mdns services to our local instance so that the c_str pointers remain valid for the lifetime of this
    // component
    this->mdns_services_ = this->mdns_->get_services();
    ESP_LOGW(TAG, "Setting up SRP services. count = %d", this->mdns_services_.size());

    for (const auto &service : this->mdns_services_) {
      otSrpClientBuffersServiceEntry *entry = otSrpClientBuffersAllocateService(instance);
      if (!entry) {
        ESP_LOGW(TAG, "Failed to allocate service entry");
        continue;
      }

      // Set service name
      char *string = otSrpClientBuffersGetServiceEntryServiceNameString(entry, &size);
      std::string full_service = service.service_type + "." + service.proto;
      if (full_service.size() > size) {
        ESP_LOGW(TAG, "Service name too long: %s", full_service.c_str());
        continue;
      }
      memcpy(string, full_service.c_str(), full_service.size() + 1);

      // Set instance name (using host_name)
      string = otSrpClientBuffersGetServiceEntryInstanceNameString(entry, &size);
      if (this->host_name_.size() > size) {
        ESP_LOGW(TAG, "Instance name too long: %s", this->host_name_.c_str());
        continue;
      }
      memcpy(string, this->host_name_.c_str(), this->host_name_.size() + 1);

      // Set port
      entry->mService.mPort = service.port;

      // Set TXT records if available
      if (!service.txt_records.empty()) {
        // Allocate memory for TXT entries
        otDnsTxtEntry *mTxtEntries =
            reinterpret_cast<otDnsTxtEntry *>(this->pool_alloc_(sizeof(otDnsTxtEntry) * service.txt_records.size()));

        // Fill TXT entries
        for (size_t i = 0; i < service.txt_records.size(); i++) {
          const auto &txt = service.txt_records[i];

          mTxtEntries[i].mKey = txt.key.c_str();
          mTxtEntries[i].mValue = reinterpret_cast<const uint8_t *>(txt.value.c_str());
          mTxtEntries[i].mValueLength = txt.value.size();
        }

        entry->mService.mTxtEntries = mTxtEntries;
        entry->mService.mNumTxtEntries = service.txt_records.size();
      } else {
        entry->mService.mNumTxtEntries = 0;
        entry->mService.mTxtEntries = nullptr;
      }

      // Add service
      error = otSrpClientAddService(instance, &entry->mService);
      if (error != OT_ERROR_NONE) {
        ESP_LOGW(TAG, "Failed to add service: %s", otThreadErrorToString(error));
      } else {
        ESP_LOGI(TAG, "Added service: %s", full_service.c_str());
      }
    }
  } else {
    // Fallback to default service if mDNS is not available
    ESP_LOGW(TAG, "mDNS component not available, using default service");

    otSrpClientBuffersServiceEntry *entry = otSrpClientBuffersAllocateService(instance);
    if (!entry) {
      ESP_LOGW(TAG, "Failed to allocate service entry for default service");
      return;
    }

    // Set service name
    char *string = otSrpClientBuffersGetServiceEntryServiceNameString(entry, &size);
    std::string full_service = "_esphome._tcp";
    if (full_service.size() > size) {
      ESP_LOGW(TAG, "Default service name too long");
      return;
    }
    memcpy(string, full_service.c_str(), full_service.size() + 1);

    // Set instance name (using host_name)
    string = otSrpClientBuffersGetServiceEntryInstanceNameString(entry, &size);
    if (this->host_name_.size() > size) {
      ESP_LOGW(TAG, "Instance name too long: %s", this->host_name_.c_str());
      return;
    }
    memcpy(string, this->host_name_.c_str(), this->host_name_.size() + 1);

    // Set port
    entry->mService.mPort = 6053;
    entry->mService.mNumTxtEntries = 0;
    entry->mService.mTxtEntries = nullptr;

    // Add service
    error = otSrpClientAddService(instance, &entry->mService);
    if (error != OT_ERROR_NONE) {
      ESP_LOGW(TAG, "Failed to add default service: %s", otThreadErrorToString(error));
    } else {
      ESP_LOGI(TAG, "Added default service: _esphome._tcp");
    }
  }

  // Enable auto start mode for SRP client
  otSrpClientEnableAutoStartMode(instance, srpStartCallback, nullptr);
  ESP_LOGI(TAG, "Finished SRP setup");
}

void OpenThreadZephyr::on_shutdown() {
  // Stop the Thread network
  this->stop_thread_network();

  // Clean up SRP services if needed
  otInstance *instance = openthread_get_default_instance();
  if (instance != nullptr) {
    // Disable SRP client
    otSrpClientStop(instance);
    ESP_LOGD(TAG, "SRP client stopped");
  }
}

network::IPAddresses OpenThreadZephyr::get_ip_addresses() {
  network::IPAddresses addresses{};
  size_t i = 0;

  if (!this->has_ipv6_address_) {
    return addresses;
  }

  // Get OpenThread instance
  otInstance *instance = openthread_get_default_instance();
  if (instance == nullptr) {
    ESP_LOGW(TAG, "OpenThread instance not available for getting IP addresses");
    return addresses;
  }

  // Get unicast addresses
  const otNetifAddress *unicast_addrs = otIp6GetUnicastAddresses(instance);

  for (const otNetifAddress *addr = unicast_addrs; addr && i < addresses.size(); addr = addr->mNext) {
    char addr_str[OT_IP6_ADDRESS_STRING_SIZE];
    otIp6AddressToString(&addr->mAddress, addr_str, sizeof(addr_str));

    // Convert to IPAddress and add to array
    addresses[i] = network::IPAddress(addr_str);
    i++;
  }

  return addresses;
}

}  // namespace zephyr_openthread
}  // namespace esphome

#endif  // defined(USE_ZEPHYR) && defined(USE_ZEPHYR_OPENTHREAD)
