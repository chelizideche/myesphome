#pragma once

#include "esphome/components/zigbee/zigbee_component.h"
#ifdef USE_ZIGBEE
#include "esphome/core/component.h"
#include "esphome/components/time/real_time_clock.h"

extern "C" {
#include <zboss_api.h>
#include <zboss_api_addons.h>
}

namespace esphome {
namespace zigbee {

class ZigbeeTime : public time::RealTimeClock, public ZigbeeEntity {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE - 1.0f; }
  void dump_config() override;
  void update() override;

  void set_cluster_attributes(zb_zcl_time_attrs_t &cluster_attributes) {
    this->cluster_attributes_ = &cluster_attributes;
  }

  void set_epoch_time(uint32_t epoch);

 protected:
  void zcl_device_cb_(zb_bufid_t bufid);
  zb_zcl_time_attrs_t *cluster_attributes_{nullptr};

  bool has_time_{false};
};

}  // namespace zigbee
}  // namespace esphome

#endif