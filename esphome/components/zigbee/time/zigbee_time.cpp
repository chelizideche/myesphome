#include "zigbee_time.h"
#ifdef USE_ZIGBEE
#include "esphome/core/log.h"

namespace esphome {
namespace zigbee {

static const char *const TAG = "zigbee.time";

// This time standard is the number of
// seconds since 0 hrs 0 mins 0 sec on 1st January 2000 UTC (Universal Coordinated Time).
constexpr time_t EPOCH_2000 = 946684800;

void ZigbeeTime::setup() { synchronize_epoch_(EPOCH_2000); }

void ZigbeeTime::dump_config() {
  ESP_LOGCONFIG(TAG, "Zigbee Time");
  ESP_LOGCONFIG(TAG, "  EP: %d", ep_);
}

void ZigbeeTime::update() {
#ifdef USE_ZIGBEE_TIME_SERVER
  time_t time = timestamp_now();
  cluster_attributes_->time = time - EPOCH_2000;
#endif
}

void ZigbeeTime::set_parent(Zigbee *parent) {
  this->parent_ = parent;
#ifdef USE_ZIGBEE_TIME_SERVER
  this->parent_->add_callback(this->ep_, [this](zb_bufid_t bufid) { this->zcl_device_cb_(bufid); });
#endif
}

#ifdef USE_ZIGBEE_TIME_SERVER
void ZigbeeTime::zcl_device_cb_(zb_bufid_t bufid) {
  zb_zcl_device_callback_param_t *p_device_cb_param = ZB_BUF_GET_PARAM(bufid, zb_zcl_device_callback_param_t);
  zb_zcl_device_callback_id_t device_cb_id = p_device_cb_param->device_cb_id;
  zb_uint16_t cluster_id = p_device_cb_param->cb_param.set_attr_value_param.cluster_id;
  zb_uint16_t attr_id = p_device_cb_param->cb_param.set_attr_value_param.attr_id;

  p_device_cb_param->status = RET_OK;

  switch (device_cb_id) {
    /* ZCL set attribute value */
    case ZB_ZCL_SET_ATTR_VALUE_CB_ID:
      if (cluster_id == ZB_ZCL_CLUSTER_ID_TIME) {
        if (attr_id == ZB_ZCL_ATTR_TIME_TIME_ID) {
          zb_uint32_t value = p_device_cb_param->cb_param.set_attr_value_param.values.data32;
          ESP_LOGI(TAG, "binary output attribute setting to %u", value);
          synchronize_epoch_(value + EPOCH_2000);
        } else if (attr_id == ZB_ZCL_ATTR_TIME_TIME_STATUS_ID) {
          zb_uint8_t value = p_device_cb_param->cb_param.set_attr_value_param.values.data8;
          ESP_LOGI(TAG, "binary output attribute setting to %hd", value);
          has_time_ = ZB_ZCL_TIME_TIME_STATUS_SYNCHRONIZED_BIT_IS_SET(value);
        }
      } else {
        /* other clusters attribute handled here */
        ESP_LOGI(TAG, "Unhandled cluster attribute id: %d", cluster_id);
      }
      break;
    default:
      p_device_cb_param->status = RET_ERROR;
      break;
  }

  ESP_LOGD(TAG, "%s status: %hd", __func__, p_device_cb_param->status);
}
#endif

}  // namespace zigbee
}  // namespace esphome

#endif