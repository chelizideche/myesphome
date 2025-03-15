#pragma once

#include "esphome/core/defines.h"

// Forward declaration of the OpenThreadZephyr class
// This ensures the class is available even when USE_ZEPHYR is not defined
namespace esphome {
namespace zephyr_openthread {
class OpenThreadZephyr;

// Add global component variable
extern OpenThreadZephyr *global_openthread_component;
}  // namespace zephyr_openthread
}  // namespace esphome

// Make the namespace available in the global scope
using namespace esphome;

#if defined(USE_ZEPHYR) && defined(USE_ZEPHYR_OPENTHREAD)

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include <string>

// Include Zephyr networking headers
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/openthread.h>

// Include OpenThread headers
#include <openthread/thread.h>
#include <openthread/srp_client.h>
#include <openthread/dns.h>

// Include mDNS component
#include "esphome/components/mdns/mdns_component.h"
#include "esphome/components/network/ip_address.h"

// Forward declaration for OpenThread types if needed
struct otDnsTxtEntry;

namespace esphome {
namespace zephyr_openthread {

class OpenThreadZephyr : public Component {
 public:
  OpenThreadZephyr() = default;
  ~OpenThreadZephyr() = default;

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::WIFI; }
  void on_shutdown() override;

  void set_channel(uint8_t channel) { channel_ = channel; }
  void set_panid(uint16_t panid) { panid_ = panid; }
  void set_network_name(const std::string &network_name) { network_name_ = network_name; }
  void set_xpanid(const std::string &xpanid) { xpanid_ = xpanid; }
  void set_network_key(const std::string &network_key) { network_key_ = network_key; }
  void set_pskc(const std::string &pskc) { pskc_ = pskc; }
  void set_radio_tx_power(int8_t tx_power) { radio_tx_power_ = tx_power; }
  void set_force_dataset(bool force_dataset) { force_dataset_ = force_dataset; }
  void set_mdns(mdns::MDNSComponent *mdns) { mdns_ = mdns; }

  bool is_connected() const { return connected_; }
  void start_thread_network();
  void stop_thread_network();

  // IPv6 related methods
  bool has_ipv6_address() const { return has_ipv6_address_; }
  const std::string &get_ipv6_address() const { return ipv6_address_; }

  // Add get_ip_addresses method
  network::IPAddresses get_ip_addresses();

 private:
  std::vector<std::unique_ptr<uint8_t[]>> memory_pool_;
  void *pool_alloc_(size_t size);

 protected:
  uint8_t channel_{15};
  uint16_t panid_{0xABCD};
  std::string network_name_{"ESPHome-OpenThread"};
  std::string xpanid_{"FEDCBA9876543210"};
  std::string network_key_{"00112233445566778899AABBCCDDEEFF"};
  std::string pskc_{"00112233445566778899AABBCCDDEEFF"};
  int8_t radio_tx_power_{0};
  bool force_dataset_{false};
  bool connected_{false};
  bool thread_started_{false};

  // IPv6 related properties
  bool has_ipv6_address_{false};
  std::string ipv6_address_{};

  // Thread role
  uint8_t thread_role_{0};
  bool srp_services_registered_{false};

  // SRP service related properties
  std::string host_name_{};
  std::vector<std::string> service_names_{};

  void update_ipv6_addresses();
  void configure_operational_dataset();
  void setup_srp_services();

  // mDNS component reference
  mdns::MDNSComponent *mdns_{nullptr};
  std::vector<mdns::MDNSService> mdns_services_{};
};

}  // namespace zephyr_openthread
}  // namespace esphome

#endif  // defined(USE_ZEPHYR) && defined(USE_ZEPHYR_OPENTHREAD)
