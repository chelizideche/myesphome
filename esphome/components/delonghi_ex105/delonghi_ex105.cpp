#include "delonghi_ex105.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/log.h"

namespace esphome {
namespace delonghi_ex105 {


void DelonghiClimate::setup() {
  this->mode = climate::CLIMATE_MODE_OFF;
  this->action = climate::CLIMATE_ACTION_OFF;
  this->publish_state();
}

static int mode_to_index(climate::ClimateMode m) {
  switch (m) {
    case climate::CLIMATE_MODE_COOL:     return 0;
    case climate::CLIMATE_MODE_DRY:      return 1;
    case climate::CLIMATE_MODE_FAN_ONLY: return 2;
    default:                              return 0;
  }
}

static int fan_mode_to_index(climate::ClimateFanMode m) {
  switch (m) {
    case climate::CLIMATE_FAN_AUTO:   return 0;
    case climate::CLIMATE_FAN_LOW:    return 1;
    case climate::CLIMATE_FAN_MEDIUM: return 2;
    case climate::CLIMATE_FAN_HIGH:   return 3;
    default:                          return 0;
  }
}

static int swing_mode_to_index(climate::ClimateSwingMode s) {
  switch (s) {
    case climate::CLIMATE_SWING_VERTICAL: return 1;
    case climate::CLIMATE_SWING_OFF:
    default:                              return 0;
  }
}

void DelonghiClimate::do_step_() {
  if (this->steps_left_ <= 0) {
    if (this->step_complete_callback_) {
      this->step_complete_callback_();
      this->step_complete_callback_ = nullptr;
    }
    return;
  }

  this->pending_command_ = this->step_command_;
  this->transmit_state();

  this->steps_left_--;
  this->set_timeout("delonghi_step", 200, [this]() { this->do_step_(); });
}


void DelonghiClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) {
  auto new_mode = *call.get_mode();
  if (new_mode == climate::CLIMATE_MODE_OFF) {
    this->pending_command_ = DELONGHI_POWER_TOGGLE;
    this->transmit_state();
    this->mode   = climate::CLIMATE_MODE_OFF;
    this->action = climate::CLIMATE_ACTION_OFF;
    this->publish_state();
    return;
  }
  int target_i = mode_to_index(new_mode);
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    this->pending_command_ = DELONGHI_POWER_TOGGLE;
    this->transmit_state();
    this->target_temperature  = this->last_target_temperature_;
    this->current_temperature = this->last_target_temperature_;
    this->mode   = new_mode;
    this->action = (new_mode == climate::CLIMATE_MODE_COOL
                      ? climate::CLIMATE_ACTION_COOLING
                      : climate::CLIMATE_ACTION_IDLE);
    this->publish_state();
    return;
  }
  int delta = (target_i - this->current_mode_index_ + 3) % 3;
  this->steps_left_          = delta;
  this->step_command_        = DELONGHI_MODE;
  this->step_complete_callback_ = [this, target_i, new_mode]() {
  this->current_mode_index_ = target_i;
  this->mode   = new_mode;
  this->action = (new_mode == climate::CLIMATE_MODE_COOL
                    ? climate::CLIMATE_ACTION_COOLING
                    : climate::CLIMATE_ACTION_IDLE);
    this->publish_state();
  };
  this->do_step_();
  return;
}
else if (call.get_target_temperature().has_value()) {
  int new_temp  = *call.get_target_temperature();
  this->last_target_temperature_ = new_temp;
  int curr_temp = int(this->target_temperature);
  int delta     = new_temp - curr_temp;         

  if (delta > 0) {
    this->steps_left_          = delta;
    this->step_command_        = DELONGHI_TEMP_UP;
    this->step_complete_callback_ = [this, new_temp]() {
      this->current_temperature = new_temp;
      this->target_temperature  = new_temp;
      this->publish_state();
    };
    this->do_step_();
    return;
  } else if (delta < 0) {
    this->steps_left_          = -delta;
    this->step_command_        = DELONGHI_TEMP_DOWN;
    this->step_complete_callback_ = [this, new_temp]() {
      this->current_temperature = new_temp;
      this->target_temperature  = new_temp;
      this->publish_state();
    };
    this->do_step_();
    return;
  }
  }
  else if (call.get_fan_mode().has_value()) {
    auto new_fan = *call.get_fan_mode();
    int target_i = fan_mode_to_index(new_fan);
    int delta = (target_i - this->current_fan_index_ + 4) % 4;
    this->steps_left_          = delta;
    this->step_command_        = DELONGHI_FAN_COMMAND;
    this->step_complete_callback_ = [this, target_i, new_fan]() {
      this->current_fan_index_ = target_i;
      this->fan_mode  = new_fan;
      this->publish_state();
    };
    this->do_step_();
    return;
  }
  else if (call.get_swing_mode().has_value()) {
    auto new_swing = *call.get_swing_mode();
    int target_i   = swing_mode_to_index(new_swing);
    int delta = (target_i - this->current_swing_index_ + 2) % 2;
    this->steps_left_          = delta;
    this->step_command_        = DELONGHI_SWING_COMMAND;
    this->step_complete_callback_ = [this, target_i, new_swing]() {
      this->current_swing_index_ = target_i;
      this->swing_mode  = new_swing;
      this->publish_state();
    };
    this->do_step_();
    return;
  }
else if (call.get_preset().has_value() &&
         *call.get_preset() == climate::CLIMATE_PRESET_NONE) {
  const auto prev = this->custom_preset.value_or("");
  this->custom_preset.reset();
  this->preset = climate::CLIMATE_PRESET_NONE;
  this->publish_state();

  uint16_t cmd = 0;
  if (prev == "Silent")           cmd = SILENT_COMMAND;
  else if (prev == "Eco Real Feel") cmd = ECO_REAL_FEEL_COMMAND;

  if (cmd) {
    this->steps_left_            = 1;
    this->step_command_          = cmd;
    this->step_complete_callback_ = []() {};
    this->do_step_();
  }
  return;
}

else if (call.get_custom_preset().has_value()) {
  const auto &cp   = *call.get_custom_preset();
  const auto  prev = this->custom_preset.value_or("");

  this->custom_preset = cp;
  this->preset.reset();
  this->publish_state();

  uint16_t cmd_off = 0;
  if (cp == "Silent" && prev == "Eco Real Feel") {
    cmd_off = ECO_REAL_FEEL_COMMAND;
  } else if (cp == "Eco Real Feel" && prev == "Silent") {
    cmd_off = SILENT_COMMAND;
  }
  uint16_t cmd_on = (cp == "Silent") ? SILENT_COMMAND : ECO_REAL_FEEL_COMMAND;

  if (cmd_off) {
    this->steps_left_   = 1;
    this->step_command_ = cmd_off;
    this->step_complete_callback_ = [this, cmd_on]() {
      this->set_timeout("preset_on", 200, [this, cmd_on]() {
        this->pending_command_ = cmd_on;
        this->transmit_state();
      });
    };
    this->do_step_();
  } else {
    this->pending_command_ = cmd_on;
    this->transmit_state();
  }
  return;
}

this->publish_state();
}


static const char *const TAG = "delonghi_ex105.climate";

void DelonghiClimate::dump_config() {
  ESP_LOGCONFIG(TAG, "Delonghi EX105 IR Climate:");
  climate_ir::ClimateIR::dump_config();

  ESP_LOGCONFIG(TAG, "  Address: 0x%04X", DELONGHI_ADDRESS);
  ESP_LOGCONFIG(TAG, "  Temperature range: %u–%u °C (step %.1f)",
                DELONGHI_TEMP_MIN, DELONGHI_TEMP_MAX, this->traits().get_visual_current_temperature_step());
  ESP_LOGCONFIG(TAG, "  IR carrier: %u Hz", static_cast<unsigned>(DELONGHI_IR_FREQUENCY));

  ESP_LOGCONFIG(TAG, "  Commands:");
  ESP_LOGCONFIG(TAG, "    Power toggle:       0x%04X", DELONGHI_POWER_TOGGLE);
  ESP_LOGCONFIG(TAG, "    Mode cycle:         0x%04X", DELONGHI_MODE);
  ESP_LOGCONFIG(TAG, "    Temp up/down:       0x%04X / 0x%04X",
                DELONGHI_TEMP_UP, DELONGHI_TEMP_DOWN);
  ESP_LOGCONFIG(TAG, "    Fan cycle:          0x%04X", DELONGHI_FAN_COMMAND);
  ESP_LOGCONFIG(TAG, "    Swing cycle:        0x%04X", DELONGHI_SWING_COMMAND);
  ESP_LOGCONFIG(TAG, "    Eco Real Feel:      0x%04X", ECO_REAL_FEEL_COMMAND);
  ESP_LOGCONFIG(TAG, "    Silent mode:        0x%04X", SILENT_COMMAND);
}



void DelonghiClimate::transmit_state() {
  uint16_t remote_state[2];
  remote_state[0] = DELONGHI_ADDRESS;
  remote_state[1] = this->pending_command_;

  auto xmt = this->transmitter_->transmit();
  auto *data = xmt.get_data();
  data->set_carrier_frequency(DELONGHI_IR_FREQUENCY);


  data->mark(DELONGHI_HEADER_MARK);
  data->space(DELONGHI_HEADER_SPACE);


  for (uint16_t word : remote_state) {
    uint8_t lsb = word & 0xFF, msb = (word >> 8) & 0xFF;
    for (uint8_t mask = 1; mask; mask <<= 1) {
      data->mark(DELONGHI_BIT_MARK);
      data->space((lsb & mask) ? DELONGHI_ONE_SPACE : DELONGHI_ZERO_SPACE);
    }
    for (uint8_t mask = 1; mask; mask <<= 1) {
      data->mark(DELONGHI_BIT_MARK);
      data->space((msb & mask) ? DELONGHI_ONE_SPACE : DELONGHI_ZERO_SPACE);
    }
  }

  data->mark(DELONGHI_BIT_MARK);
  data->space(0);

  xmt.perform();
}


}  // namespace delonghi_ex105
}  // namespace esphome
