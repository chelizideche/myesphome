#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cst328 {

static const char *const TAG = "cst328.touchscreen";

static const uint8_t CST328_REG_STATUS = 0x00;
static const uint8_t CST328_TOUCH_MAX_POINTS = 5;
static const uint8_t CST328_TOUCH_DATA_SIZE = 27;

enum class Cst328Registers : uint16_t {
  TOUCH_INFORMATION = 0xD000,
  TOUCH_FINGER_NUMBER = 0xD005,
  KEY_TX_RX_NUMBERS = 0xD1F4,
  X_Y_RESOLUTION = 0xD1F8,
  FW_CRC_AND_BOOT_TIME = 0xD1FC,
  CHIP_TYPE_AND_PROJECT_ID = 0xD204,
  FW_REVISION = 0xD208,
};

enum class Cst328WorkModes : uint16_t {
  DEBUG_INFO_MODE = 0xD101,
  RESET_MODE = 0xD102,
  RECALIBRATION_MODE = 0xD104,
  DEEP_SLEEP_MODE = 0xD105,
  DEBUG_POINT_MODE = 0xD108,
  NORMAL_MODE = 0xD109,
  DEBUG_RAWDATA_MODE = 0xD10A,
  DEBUG_DIFF_MODE = 0xD10D,
  DEBUG_FACTORY_MODE = 0xD119,
  DEBUG_FACTORY_MODE_2 = 0xD120,
};

class CST328ButtonListener {
 public:
  virtual void update_button(bool state) = 0;
};

class CST328Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void update_touches() override;
  void register_button_listener(CST328ButtonListener *listener) { this->button_listeners_.push_back(listener); }
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

  bool can_proceed() override { return this->setup_complete_ || this->is_failed(); }

 protected:
  bool read16_(uint16_t addr, uint8_t *data, size_t len);
  void continue_setup_();
  void update_button_state_(bool state);

  InternalGPIOPin *interrupt_pin_{};
  GPIOPin *reset_pin_{};

  std::vector<CST328ButtonListener *> button_listeners_;
  bool button_touched_{};

  uint16_t chip_id_{};
  uint16_t project_id_{};
  uint8_t fw_ver_major_{};
  uint8_t fw_ver_minor_{};
  uint16_t fw_build_{};

  bool setup_complete_{};
};

}  // namespace cst328
}  // namespace esphome
