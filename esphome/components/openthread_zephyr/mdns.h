#pragma once

#ifdef USE_ZEPHYR

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/mdns/mdns_component.h"
#include <string>
#include <map>

namespace esphome {
namespace openthread_zephyr {

/**
 * @brief Bridge between ESPHome's mDNS component and OpenThread's SRP client.
 * 
 * This component intercepts mDNS service registrations and forwards them to
 * OpenThread's SRP client, which handles service discovery in Thread networks.
 */
class OpenThreadMDNSBridge : public Component {
 public:
  OpenThreadMDNSBridge() = default;

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

  /**
   * @brief Set the mDNS component reference.
   * 
   * @param mdns The mDNS component to bridge with OpenThread SRP.
   */
  void set_mdns(mdns::MDNSComponent *mdns) { mdns_ = mdns; }

  /**
   * @brief Register a service with OpenThread SRP.
   * 
   * @param service_type The service type (e.g., "_esphomelib").
   * @param proto The protocol (e.g., "_tcp").
   * @param port The port number.
   * @param txt_records Map of TXT record key-value pairs.
   * @return true if registration was successful, false otherwise.
   */
  bool register_service(const std::string &service_type, const std::string &proto, uint16_t port,
                       const std::map<std::string, std::string> &txt_records);

 protected:
  mdns::MDNSComponent *mdns_{nullptr};
  std::string hostname_;
  bool services_registered_{false};
  
  /**
   * @brief Sync services from mDNS component to OpenThread SRP.
   */
  void sync_services_();
};

}  // namespace openthread_zephyr
}  // namespace esphome

#endif  // USE_ZEPHYR 