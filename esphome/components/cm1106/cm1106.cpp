#include "cm1106.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace cm1106 {

static const char *const TAG = "cm1106";
static const uint8_t C_M1106_CMD_GET_CO2[4] = {0x11, 0x01, 0x01, 0xED};
static const uint8_t C_M1106_CMD_SET_CO2_CALIB[6] = {0x11, 0x03, 0x03, 0x00, 0x00, 0x00};
static const uint8_t C_M1106_CMD_SET_CO2_CALIB_RESPONSE[4] = {0x16, 0x01, 0x03, 0xE6};
static const uint8_t C_M1106_CMD_SET_ABC_STATUS[10] = {0x11, 0x07, 0x10, 0x64, 0x00, 0x0F, 0x01, 0x90, 0x64, 0x00};
static const uint8_t C_M1106_CMD_SET_ABC_STATUS_RESPONSE[4] = {0x16, 0x01, 0x10, 0xD9};


uint8_t cm1106_checksum(const uint8_t *response, size_t len) {
  uint8_t crc = 0;
  for (uint8_t i = 0; i < len - 1; i++) {
    crc -= response[i];
  }
  return crc;
}

void CM1106Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CM1106...");
  if (this->abc_boot_logic_ == CM1106_ABC_ENABLED) {
    this->abc_enable();
  } else if (this->abc_boot_logic_ == CM1106_ABC_DISABLED) {
    this->abc_disable();
  }
}

void CM1106Component::update() {
  uint8_t response[8] = {0};
  if (!this->cm1106_write_command_(C_M1106_CMD_GET_CO2, sizeof(C_M1106_CMD_GET_CO2), response, sizeof(response))) {
    ESP_LOGW(TAG, "Reading data from CM1106 failed!");
    this->status_set_warning();
    return;
  }

  if (response[0] != 0x16 || response[1] != 0x05 || response[2] != 0x01) {
    ESP_LOGW(TAG, "Got wrong UART response from CM1106: %02X %02X %02X %02X...", response[0], response[1], response[2],
             response[3]);
    this->status_set_warning();
    return;
  }

  uint8_t checksum = cm1106_checksum(response, sizeof(response));
  if (response[7] != checksum) {
    ESP_LOGW(TAG, "CM1106 Checksum doesn't match: 0x%02X!=0x%02X", response[7], checksum);
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();

  const uint16_t ppm = response[3] << 8 | response[4];
  const uint8_t status = response[5];

  if (status) {
    const bool preheating = status & 0x1;
    if (preheating) {
      ESP_LOGW(TAG, "CM1106 warming up, CO₂=%uppm", ppm);
    } else {
      ESP_LOGW(TAG, "CM1106 returned error, status=%02X", status);
    }
    this->status_set_warning();
    return;
  }

  ESP_LOGD(TAG, "CM1106 Received CO₂=%uppm status=%02X DF4=%02X", ppm, status, response[6]);
  if (this->co2_sensor_ != nullptr)
    this->co2_sensor_->publish_state(ppm);
}

void CM1106Component::calibrate_zero(uint16_t ppm) {
  uint8_t cmd[6];
  memcpy(cmd, C_M1106_CMD_SET_CO2_CALIB, sizeof(cmd));
  cmd[3] = ppm >> 8;
  cmd[4] = ppm & 0xFF;
  uint8_t response[4] = {0};

  if (!this->cm1106_write_command_(cmd, sizeof(cmd), response, sizeof(response))) {
    ESP_LOGW(TAG, "Send calibrate zero command to CM1106 failed!");
    this->status_set_warning();
    return;
  }

  // check if correct response received
  if (memcmp(response, C_M1106_CMD_SET_CO2_CALIB_RESPONSE, sizeof(response)) != 0) {
    ESP_LOGW(TAG, "Got wrong UART response from CM1106: %02X %02X %02X %02X", response[0], response[1], response[2],
             response[3]);
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();
  ESP_LOGD(TAG, "CM1106 Successfully calibrated sensor to %uppm", ppm);
}

void CM1106Component::send_abc_command(uint8_t flag) {
  uint8_t cmd[10];
  memcpy(cmd, C_M1106_CMD_SET_ABC_STATUS, sizeof(cmd));
  cmd[4] = flag;
  uint8_t response[4] = {0};

  if (!this->cm1106_write_command_(cmd, sizeof(cmd), response, sizeof(response))) {
    ESP_LOGW(TAG, "Send ABC command to CM1106 failed!");
    this->status_set_warning();
    return;
  }

  // check if correct response received
  if (memcmp(response, C_M1106_CMD_SET_ABC_STATUS_RESPONSE, sizeof(response)) != 0) {
    ESP_LOGW(TAG, "Got wrong UART response from CM1106: %02X %02X %02X %02X", response[0], response[1], response[2],
             response[3]);
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();
  ESP_LOGD(TAG, "CM1106 Successfully set ABC status");
}

void CM1106Component::abc_enable() {
  ESP_LOGD(TAG, "CM1106 Enabling automatic baseline calibration");
  this->send_abc_command(0x0);
}

void CM1106Component::abc_disable() {
  ESP_LOGD(TAG, "CM1106 Disabling automatic baseline calibration");
  this->send_abc_command(0x2);
}

bool CM1106Component::cm1106_write_command_(const uint8_t *command, size_t command_len, uint8_t *response,
                                            size_t response_len) {
  // Empty RX Buffer
  while (this->available())
    this->read();
  this->write_array(command, command_len - 1);
  this->write_byte(cm1106_checksum(command, command_len));
  this->flush();

  if (response == nullptr)
    return true;

  return this->read_array(response, response_len);
}

void CM1106Component::dump_config() {
  ESP_LOGCONFIG(TAG, "CM1106:");
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  this->check_uart_settings(9600);

  if (this->abc_boot_logic_ == CM1106_ABC_ENABLED) {
    ESP_LOGCONFIG(TAG, "  Automatic baseline calibration enabled on boot");
  } else if (this->abc_boot_logic_ == CM1106_ABC_DISABLED) {
    ESP_LOGCONFIG(TAG, "  Automatic baseline calibration disabled on boot");
  }
}

}  // namespace cm1106
}  // namespace esphome
