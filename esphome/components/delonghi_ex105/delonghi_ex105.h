#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace delonghi_ex105 {

// Values for DELONGHI EX105 IR Controllers
static constexpr uint16_t DELONGHI_ADDRESS = 0xFF1F;

// Temperature
static constexpr uint16_t DELONGHI_TEMP_MIN = 18;
static constexpr uint16_t DELONGHI_TEMP_MAX = 32;          

// Modes
static constexpr uint16_t DELONGHI_MODE = 0x3EC1;
static constexpr uint16_t DELONGHI_POWER_TOGGLE = 0x7C83;

// Fan Speed
static constexpr uint16_t DELONGHI_FAN_COMMAND = 0x3FC0;

//Fan Swinging
static constexpr uint16_t DELONGHI_SWING_COMMAND = 0x7A85;

//Temp Control
static constexpr uint16_t DELONGHI_TEMP_UP = 0x7B84;
static constexpr uint16_t DELONGHI_TEMP_DOWN = 0x7F80;

//Custom Delonghi Buttons
static constexpr uint16_t ECO_REAL_FEEL_COMMAND = 0x7D82;
static constexpr uint16_t SILENT_COMMAND = 0x7986;

// IR Transmission - similar to NEC1
static constexpr uint32_t DELONGHI_IR_FREQUENCY = 38000;
static constexpr uint32_t DELONGHI_HEADER_MARK = 9000;
static constexpr uint32_t DELONGHI_HEADER_SPACE = 4500;
static constexpr uint32_t DELONGHI_BIT_MARK = 465;
static constexpr uint32_t DELONGHI_ONE_SPACE = 1750;
static constexpr uint32_t DELONGHI_ZERO_SPACE = 670;

// State Frame size
static constexpr uint8_t DELONGHI_STATE_FRAME_SIZE = 8;

class DelonghiClimate : public climate_ir::ClimateIR {
 public:
  void dump_config() override;
  DelonghiClimate()
      : climate_ir::ClimateIR(DELONGHI_TEMP_MIN, DELONGHI_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                               climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH}) {}


 protected:
  int current_mode_index_{0};
  float last_target_temperature_{20.0f};
  int current_fan_index_{0};
  int current_swing_index_{0};

  void control(const climate::ClimateCall &call) override;
  void transmit_state() override;
  void setup() override;
  uint16_t pending_command_{ DELONGHI_POWER_TOGGLE };
    climate::ClimateTraits traits() override {
      climate::ClimateTraits t{};
      t.set_supported_modes({
        climate::CLIMATE_MODE_OFF,
        climate::CLIMATE_MODE_DRY,
        climate::CLIMATE_MODE_COOL,
        climate::CLIMATE_MODE_FAN_ONLY  
      });
      t.set_visual_temperature_step(1.0f);
      t.set_supported_fan_modes({
        climate::CLIMATE_FAN_AUTO,
        climate::CLIMATE_FAN_LOW,
        climate::CLIMATE_FAN_MEDIUM,
        climate::CLIMATE_FAN_HIGH,
      });
      t.set_supported_swing_modes({
        climate::CLIMATE_SWING_VERTICAL,
        climate::CLIMATE_SWING_OFF
      });
      t.set_supported_presets({
        climate::CLIMATE_PRESET_NONE
      });
      t.set_supported_custom_presets({"Silent", "Eco Real Feel"});
      return t;
    }

};
}  // namespace delonghi_ex105 
}  // namespace esphome
