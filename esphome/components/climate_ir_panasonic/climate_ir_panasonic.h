#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

#include <cinttypes>

namespace esphome {
namespace climate_ir_panasonic {

/*
Based on reversing https://github.com/crankyoldgit/IRremoteESP8266/blob/master/src/ir_Panasonic.cpp
Panasonic uses a 216bit frame The structure is as follows:
B0-B12:
  Static: 0x0220E004000000060220E00400
B13:
  b0:   Power state
  b1:   Timer enabled
  b4-6: Mode
B14:
  b1-5: Temperature set-point

B16:
  b0-3: Vanes vertical position
  b4-7: Fan speed
B17:
  Lke, Nke, Dke, Rkr
    b0-3: horizontal vane position
  Seems to also be used in conjunction with B21, and B23 to determine AC model
B18:
  OnTimer minutes_since_midnight (round down to nearest 10min)
  Max value is 23h and 59min.
  A value of 0x600 has some special meaning. Unclear
B19:
  b0-2: 3 MSb of time from above.
  b4-7: 4 LSb of the Offtimer
B20:
  b0-6: 7 MSb of the Offtimer
B21:
  RKR CKP:
    b0: powerful
    b5: quiet
  CKP:
    b4: constant 1
  Others:
    b0: quiet
    b5: powerful

B23:
  varies between models
B24:
  Time in minutes since midnight (not rounded down)
  Max value is 23h and 59min.
  A value of 0x600 has some special meaning. Unclear
B25:
  b0-2: 3 MSb of time from above.
B26:
  Checksum. LSB of sum of all previous bytes
*/

// IR params
const uint16_t HEADER_MARK = 3500;    // [uS]
const uint16_t HEADER_SPACE = 1800;   // [uS]
const uint16_t BIT_MARK = 433;        // [uS]
const uint16_t BIT_ONE_SPACE = 1300;  // [uS]
const uint16_t BIT_ZERO_SPACE = 433;  // [uS]
const uint16_t SECTION_GAP = 10000;   // [uS]

// Temperature
const uint8_t TEMP_MIN = 16;  // Celsius
const uint8_t TEMP_MAX = 30;  // Celsius

enum Model : uint8_t { DKE, JKE, LKE, NKE, CKP, RKR, PKE };

enum Mode : uint8_t { AUTO = 0, DRY = 2, COOL = 3, HEAT = 4, FAN = 6 };

enum FanSpeed : uint8_t { FAN_MINIMUM = 3, FAN_LOW = 4, FAN_MEDIUM = 5, FAN_HIGH = 6, FAN_MAXIMUM = 7, FAN_AUTO = 10 };

enum ClimateVanesVertical : uint8_t {
  VANE_AUTO = 0xFFFF,
  VANE_UP = 1,
  VANE_MID_UP = 2,
  VANE_MIDDLE_VERTICAL = 3,
  VANE_MID_DOWN = 4,
  VANE_DOWN = 5
};

class PanasonicIrClimate : public climate_ir::ClimateIR {
 public:
  PanasonicIrClimate()
      : climate_ir::ClimateIR(TEMP_MIN, TEMP_MAX,
                              1.0f,  // Temp step
                              true,  // Supports dry
                              true,  // Supports fan only
                              {
                                  // Supported fan modes/speeds
                                  climate::CLIMATE_FAN_AUTO,
                                  climate::CLIMATE_FAN_MINIMUM,
                                  climate::CLIMATE_FAN_LOW,
                                  climate::CLIMATE_FAN_MEDIUM,
                                  climate::CLIMATE_FAN_HIGH,
                                  climate::CLIMATE_FAN_MAXIMUM,
                              },
                              {// Supported swing modes
                               climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {}

 protected:
  // Basic state to base messages on
  const uint8_t pansonicAC_state_base_[27]{0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x06, 0x02,
                                           0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00,
                                           0x00, 0x0E, 0xE0, 0x00, 0x00, 0x81, 0x00, 0x00, 0x00};

  Model model_;
  bool CKP_is_on_ = false;  // This is used to try and keep track of the state for a CKP model

  uint8_t PanasonicIrClimate::calcChecksum_(uint8_t state[], size_t len);

  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
};

}  // namespace climate_ir_panasonic
}  // namespace esphome
