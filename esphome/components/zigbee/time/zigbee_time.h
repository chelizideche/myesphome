#pragma once

#include "esphome/components/zigbee/zigbee_component.h"
#ifdef USE_ZIGBEE
#include "esphome/core/component.h"
#include "esphome/components/time/real_time_clock.h"

extern "C" {
#include <zboss_api.h>
#include <zboss_api_addons.h>
}

#ifdef USE_ZIGBEE_TIME_SERVER

#define ESPHOME_ZB_HA_DECLARE_TIME_CLUSTER_LIST(cluster_list_name, time_attr_list, basic_attr_list, \
                                                identify_attr_list, groups_attr_list, scenes_attr_list) \
  zb_zcl_cluster_desc_t cluster_list_name[] = { \
      ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_IDENTIFY, ZB_ZCL_ARRAY_SIZE(identify_attr_list, zb_zcl_attr_t), \
                          (identify_attr_list), ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID), \
      ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_BASIC, ZB_ZCL_ARRAY_SIZE(basic_attr_list, zb_zcl_attr_t), \
                          (basic_attr_list), ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID), \
      ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_TIME, ZB_ZCL_ARRAY_SIZE(time_attr_list, zb_zcl_attr_t), (time_attr_list), \
                          ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID), \
      ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_GROUPS, ZB_ZCL_ARRAY_SIZE(groups_attr_list, zb_zcl_attr_t), \
                          (groups_attr_list), ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID), \
      ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_SCENES, ZB_ZCL_ARRAY_SIZE(scenes_attr_list, zb_zcl_attr_t), \
                          (scenes_attr_list), ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID)}

#define ESPHOME_ZB_HA_DEVICE_VER_SIMPLE_SENSOR 0

#define ESPHOME_ZB_ZCL_DECLARE_TIME_SIMPLE_DESC(ep_name, ep_id, in_clust_num, out_clust_num) \
  ESPHOME_ZB_DECLARE_SIMPLE_DESC(ep_name, in_clust_num, out_clust_num); \
  ESPHOME_ZB_AF_SIMPLE_DESC_TYPE(ep_name, in_clust_num, out_clust_num) \
  simple_desc_##ep_name = {ep_id, \
                           ZB_AF_HA_PROFILE_ID, \
                           ZB_HA_CUSTOM_ATTR_DEVICE_ID, \
                           ESPHOME_ZB_HA_DEVICE_VER_SIMPLE_SENSOR, \
                           0, \
                           in_clust_num, \
                           out_clust_num, \
                           {ZB_ZCL_CLUSTER_ID_BASIC, ZB_ZCL_CLUSTER_ID_IDENTIFY, ZB_ZCL_CLUSTER_ID_TIME, \
                            ZB_ZCL_CLUSTER_ID_SCENES, ZB_ZCL_CLUSTER_ID_GROUPS}}

#define ESPHOME_ZB_HA_TIME_IN_CLUSTER_NUM 5   // server roles in ESPHOME_ZB_HA_DECLARE_TIME_CLUSTER_LIST
#define ESPHOME_ZB_HA_TIME_OUT_CLUSTER_NUM 0  // client roles in ESPHOME_ZB_HA_DECLARE_TIME_CLUSTER_LIST

#define ESPHOME_ZB_HA_DECLARE_TIME_EP(ep_name, ep_id, cluster_list) \
  ESPHOME_ZB_ZCL_DECLARE_TIME_SIMPLE_DESC(ep_name, ep_id, ESPHOME_ZB_HA_TIME_IN_CLUSTER_NUM, \
                                          ESPHOME_ZB_HA_TIME_OUT_CLUSTER_NUM); \
  ZBOSS_DEVICE_DECLARE_REPORTING_CTX(reporting_info##ep_name, ZB_ZCL_TIME_REPORT_ATTR_COUNT); \
  ZB_AF_DECLARE_ENDPOINT_DESC(ep_name, ep_id, ZB_AF_HA_PROFILE_ID, 0, NULL, \
                              ZB_ZCL_ARRAY_SIZE(cluster_list, zb_zcl_cluster_desc_t), cluster_list, \
                              (zb_af_simple_desc_1_1_t *) &simple_desc_##ep_name, ZB_ZCL_TIME_REPORT_ATTR_COUNT, \
                              reporting_info##ep_name, 0, NULL)

#endif

namespace esphome {
namespace zigbee {

class ZigbeeTime : public time::RealTimeClock {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE - 1.0f; }
  void dump_config() override;
  void update() override;

  void set_parent(Zigbee *parent);
  void set_ep(zb_uint8_t ep) { this->ep_ = ep; }
#ifdef USE_ZIGBEE_TIME_SERVER
  void set_cluster_attributes(zb_zcl_time_attrs_t &cluster_attributes) {
    this->cluster_attributes_ = &cluster_attributes;
  }
#endif
 protected:
#ifdef USE_ZIGBEE_TIME_SERVER
  void zcl_device_cb_(zb_bufid_t bufid);
  zb_zcl_time_attrs_t *cluster_attributes_{nullptr};
#endif

  zb_uint8_t ep_{0};
  Zigbee *parent_{nullptr};
  bool has_time_{false};
};

}  // namespace zigbee
}  // namespace esphome

#endif