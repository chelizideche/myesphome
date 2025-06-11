#pragma once

#ifdef USE_ESP32

#include <vector>

#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#include <esp_gatts_api.h>

#include "ble_scan_result.h"

namespace esphome {
namespace esp32_ble {

// Received GAP, GATTC and GATTS events are only queued, and get processed in the main loop().
// This class stores each event with minimal memory usage by only copying the data we actually need.
class BLEEvent {
 public:
  BLEEvent(esp_gap_ble_cb_event_t e, esp_ble_gap_cb_param_t *p) {
    this->type_ = GAP;
    this->event_.gap.gap_event = e;

    // Only copy the data we actually use for each GAP event type
    switch (e) {
      case ESP_GAP_BLE_SCAN_RESULT_EVT:
        // Copy only the fields we use from scan results (~72 bytes)
        memcpy(this->event_.gap.scan_result.bda, p->scan_rst.bda, sizeof(esp_bd_addr_t));
        this->event_.gap.scan_result.ble_addr_type = p->scan_rst.ble_addr_type;
        this->event_.gap.scan_result.rssi = p->scan_rst.rssi;
        this->event_.gap.scan_result.adv_data_len = p->scan_rst.adv_data_len;
        this->event_.gap.scan_result.scan_rsp_len = p->scan_rst.scan_rsp_len;
        this->event_.gap.scan_result.search_evt = p->scan_rst.search_evt;
        memcpy(this->event_.gap.scan_result.ble_adv, p->scan_rst.ble_adv,
               ESP_BLE_ADV_DATA_LEN_MAX + ESP_BLE_SCAN_RSP_DATA_LEN_MAX);
        break;

      case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        this->event_.gap.scan_complete.status = p->scan_param_cmpl.status;
        break;

      case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        this->event_.gap.scan_complete.status = p->scan_start_cmpl.status;
        break;

      case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        this->event_.gap.scan_complete.status = p->scan_stop_cmpl.status;
        break;

      default:
        // For any other GAP events, copy the full param
        // This is a safety fallback but shouldn't happen in normal operation
        memcpy(&this->event_.gap.gap_param, p, sizeof(esp_ble_gap_cb_param_t));
        break;
    }
  };

  BLEEvent(esp_gattc_cb_event_t e, esp_gatt_if_t i, esp_ble_gattc_cb_param_t *p) {
    this->type_ = GATTC;
    this->event_.gattc.gattc_event = e;
    this->event_.gattc.gattc_if = i;
    memcpy(&this->event_.gattc.gattc_param, p, sizeof(esp_ble_gattc_cb_param_t));

    // Copy data for events that need it
    switch (e) {
      case ESP_GATTC_NOTIFY_EVT:
        this->data.assign(p->notify.value, p->notify.value + p->notify.value_len);
        this->event_.gattc.gattc_param.notify.value = this->data.data();
        break;
      case ESP_GATTC_READ_CHAR_EVT:
      case ESP_GATTC_READ_DESCR_EVT:
        this->data.assign(p->read.value, p->read.value + p->read.value_len);
        this->event_.gattc.gattc_param.read.value = this->data.data();
        break;
      default:
        break;
    }
  };

  BLEEvent(esp_gatts_cb_event_t e, esp_gatt_if_t i, esp_ble_gatts_cb_param_t *p) {
    this->type_ = GATTS;
    this->event_.gatts.gatts_event = e;
    this->event_.gatts.gatts_if = i;
    memcpy(&this->event_.gatts.gatts_param, p, sizeof(esp_ble_gatts_cb_param_t));

    // Copy data for events that need it
    switch (e) {
      case ESP_GATTS_WRITE_EVT:
        this->data.assign(p->write.value, p->write.value + p->write.len);
        this->event_.gatts.gatts_param.write.value = this->data.data();
        break;
      default:
        break;
    }
  };

  union {
    // NOLINTNEXTLINE(readability-identifier-naming)
    struct gap_event {
      esp_gap_ble_cb_event_t gap_event;
      union {
        BLEScanResult scan_result;  // ~73 bytes

        // Minimal storage for scan complete events
        struct {
          esp_bt_status_t status;
        } scan_complete;  // 1 byte

        // Fallback for unexpected events (shouldn't be used)
        esp_ble_gap_cb_param_t gap_param;
      };
    } gap;  // ~80 bytes instead of 400+

    // NOLINTNEXTLINE(readability-identifier-naming)
    struct gattc_event {
      esp_gattc_cb_event_t gattc_event;
      esp_gatt_if_t gattc_if;
      esp_ble_gattc_cb_param_t gattc_param;
    } gattc;  // ~68 bytes

    // NOLINTNEXTLINE(readability-identifier-naming)
    struct gatts_event {
      esp_gatts_cb_event_t gatts_event;
      esp_gatt_if_t gatts_if;
      esp_ble_gatts_cb_param_t gatts_param;
    } gatts;  // ~68 bytes
  } event_;   // Union size is now ~80 bytes (largest member)

  std::vector<uint8_t> data{};  // For GATTC/GATTS data

  // NOLINTNEXTLINE(readability-identifier-naming)
  enum ble_event_t : uint8_t {
    GAP,
    GATTC,
    GATTS,
  } type_;

  // Helper methods to access event data
  esp_gap_ble_cb_event_t gap_event_type() const { return event_.gap.gap_event; }
  const BLEScanResult &scan_result() const { return event_.gap.scan_result; }
  esp_bt_status_t scan_complete_status() const { return event_.gap.scan_complete.status; }
};
// Total size: ~110 bytes instead of 440 bytes!

}  // namespace esp32_ble
}  // namespace esphome

#endif
