#include "emerio_pac_125152.h"
#include "esphome/core/log.h"

namespace esphome {
namespace emerio_pac_125152 {

static const char *const TAG = "climate.emerio_pac_125152";

optional<EmerioPac125152ClimateDeviceRestoreState> EmerioPac125152Climate::restore_emerio_pac_state_() {
  this->emeriopac_rtc_ = global_preferences->make_preference<EmerioPac125152ClimateDeviceRestoreState>(
      this->get_object_id_hash() ^ RESTORE_STATE_VERSION);
  EmerioPac125152ClimateDeviceRestoreState recovered{};
  if (!this->emeriopac_rtc_.load(&recovered))
    return {};

  ESP_LOGD(TAG, "Restored state: prev_mode=%d, prev_fan=%d, prev_temp=%.1f, prev_dehumidify=%s, prev_on_off=%s",
           to_internal_mode(recovered.prev_mode), to_internal_fan(recovered.prev_fan), recovered.prev_temp,
           recovered.prev_dehumidify ? "true" : "false", recovered.prev_on_off ? "true" : "false");
  return recovered;
}

void EmerioPac125152Climate::save_emerio_pac_state_() {
#if (defined(USE_ESP_IDF) || (defined(USE_ESP8266) && USE_ARDUINO_VERSION_CODE >= VERSION_CODE(3, 0, 0))) && \
    !defined(CLANG_TIDY)
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#define TEMP_IGNORE_MEMACCESS
#endif
  EmerioPac125152ClimateDeviceRestoreState state{};
  // initialize as zero to prevent random data on stack triggering erase
  memset(&state, 0, sizeof(EmerioPac125152ClimateDeviceRestoreState));
#ifdef TEMP_IGNORE_MEMACCESS
#pragma GCC diagnostic pop
#undef TEMP_IGNORE_MEMACCESS
#endif

  // Set the state values
  state.prev_mode = this->mode_before_;
  state.prev_fan = this->fan_mode_before_;
  state.prev_temp = this->target_temperature_before_;
  state.prev_dehumidify = (this->mode_before_ == climate::CLIMATE_MODE_DRY);
  state.prev_on_off = this->prev_on_off_;

  ESP_LOGD(TAG, "Saving state: prev_mode=%d, prev_fan=%d, prev_temp=%.1f, prev_dehumidify=%s, prev_on_off=%s",
           to_internal_mode(state.prev_mode), to_internal_fan(state.prev_fan), state.prev_temp,
           state.prev_dehumidify ? "true" : "false", state.prev_on_off ? "true" : "false");

  if (this->emeriopac_rtc_.save(&state)) {
    ESP_LOGD(TAG, "Saved state successfully");
  } else {
    ESP_LOGE(TAG, "Failed to save state");
  }
}

EmerioPac125152Climate::EmerioPac125152Climate()
    : climate_ir::ClimateIR(TEMP_MIN, TEMP_MAX, 1.0f, true, true,
                            {climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH}) {}

void EmerioPac125152Climate::setup() {
  // Initialize base class
  climate_ir::ClimateIR::setup();

  // Set initial state from restore or defaults
  optional<EmerioPac125152ClimateDeviceRestoreState> state = this->restore_emerio_pac_state_();
  if (state.has_value()) {
    this->mode_before_ = state->prev_mode;
    this->fan_mode_before_ = state->prev_fan;
    this->target_temperature_before_ = state->prev_temp;
    this->prev_dehumidify_ = state->prev_dehumidify;
    this->prev_on_off_ = state->prev_on_off;
  } else {
    // Default state
    this->mode_before_ = climate::CLIMATE_MODE_OFF;
    this->fan_mode_before_ = climate::CLIMATE_FAN_AUTO;
    this->target_temperature_before_ = TEMP_MIN;
    this->prev_dehumidify_ = false;
    this->prev_on_off_ = false;
  }
  ESP_LOGD(TAG,
           "Initial state: mode_before_=%d, fan_mode_before_=%d, target_temperature_before_=%.1f, prev_dehumidify_=%s, "
           "prev_on_off_=%s",
           to_internal_mode(this->mode_before_), to_internal_fan(this->fan_mode_before_),
           this->target_temperature_before_, this->prev_dehumidify_ ? "true" : "false",
           this->prev_on_off_ ? "true" : "false");
}

climate::ClimateTraits EmerioPac125152Climate::traits() {
  auto traits = climate_ir::ClimateIR::traits();
  traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_AUTO, climate::CLIMATE_MODE_COOL,
                              climate::CLIMATE_MODE_DRY, climate::CLIMATE_MODE_FAN_ONLY});
  return traits;
}

void EmerioPac125152Climate::send_nec_command_(uint16_t command) {
  // We send 3 repeats to ensure it actually receives the command
  this->transmit_<remote_base::NECProtocol>({ADDRESS, command, 3});
  // Sleep for a short time to allow the device to process the command
  delay(20);
}

void EmerioPac125152Climate::transmit_state() {
  // Save previous state for cycling math
  climate::ClimateMode prev_mode = this->mode_before_;
  ESP_LOGD(TAG, "Previous mode: %d", to_internal_mode(prev_mode));
  climate::ClimateFanMode prev_fan = this->fan_mode_before_;
  ESP_LOGD(TAG, "Previous fan: %d", to_internal_fan(prev_fan));
  bool prev_dehumidify = this->prev_dehumidify_;
  ESP_LOGD(TAG, "Previous dehumidify: %s", prev_dehumidify ? "true" : "false");
  float prev_temp = this->target_temperature_before_;
  ESP_LOGD(TAG, "Previous temperature: %.1f", prev_temp);
  ESP_LOGD(TAG, "Previous on/off state: %s", this->prev_on_off_ ? "true" : "false");

  // Requested state
  climate::ClimateMode requested_mode = this->mode;
  ESP_LOGD(TAG, "Requested mode: %d", to_internal_mode(requested_mode));
  climate::ClimateFanMode requested_fan = this->fan_mode.value();
  ESP_LOGD(TAG, "Requested fan: %d", to_internal_fan(requested_fan));
  bool requested_dehumidify = (this->mode == climate::CLIMATE_MODE_DRY);
  ESP_LOGD(TAG, "Requested dehumidify: %s", requested_dehumidify ? "true" : "false");
  float requested_temp = this->target_temperature;
  ESP_LOGD(TAG, "Requested temperature: %.1f", requested_temp);

  // Handle power
  if (requested_mode == climate::CLIMATE_MODE_OFF && this->prev_on_off_) {
    // We need to also reset the mode to our default to MODE_AUTO otherwise we will get out of sync with the device
    int steps = (to_internal_mode(climate::CLIMATE_MODE_AUTO) - to_internal_mode(prev_mode) + MODE_COUNT) % MODE_COUNT;
    ESP_LOGD(TAG, "Cycling modes: %d steps from %d to %d", steps, to_internal_mode(prev_mode),
             to_internal_mode(climate::CLIMATE_MODE_AUTO));
    for (int i = 0; i < steps; i++) {
      ESP_LOGD(TAG, "Sending mode toggle command");
      send_nec_command_(CMD_MODE);
    }

    ESP_LOGD(TAG, "Turning off climate");
    send_nec_command_(CMD_POWER);

    this->prev_on_off_ = false;  // Set previous state to OFF
    this->publish_state();
    this->save_emerio_pac_state_();
    return;
  } else {
    if (!this->prev_on_off_) {
      ESP_LOGD(TAG, "Turning on climate from OFF state");
      send_nec_command_(CMD_POWER);  // Turn on if previously off
    }
  }

  // Handle dehumidify (DRY) toggle
  if (requested_dehumidify != prev_dehumidify) {
    ESP_LOGD(TAG, "Toggling dehumidify mode");
    send_nec_command_(CMD_DEHUMIDIFY_TOGGLE);
    // If entering DRY, skip other mode/fan/temperature changes
    if (requested_dehumidify) {
      ESP_LOGD(TAG, "Entering DRY mode, skipping other changes");
      this->prev_dehumidify_ = requested_dehumidify;
      this->publish_state();
      this->save_emerio_pac_state_();
      return;
    }
    // If leaving DRY, fall through to set new mode
    ESP_LOGD(TAG, "Leaving DRY mode, continuing with other changes");
  }

  // Handle mode cycling (AUTO/COOL/FAN)
  if (requested_mode != climate::CLIMATE_MODE_DRY) {
    int steps = (to_internal_mode(requested_mode) - to_internal_mode(prev_mode) + MODE_COUNT) % MODE_COUNT;
    ESP_LOGD(TAG, "Cycling modes: %d steps from %d to %d", steps, to_internal_mode(prev_mode),
             to_internal_mode(requested_mode));
    for (int i = 0; i < steps; i++) {
      ESP_LOGD(TAG, "Sending mode toggle command");
      send_nec_command_(CMD_MODE);
    }
  }

  // Handle fan cycling
  int fan_steps = (to_internal_fan(requested_fan) - to_internal_fan(prev_fan) + FAN_COUNT) % FAN_COUNT;
  ESP_LOGD(TAG, "Cycling fans: %d steps from %d to %d", fan_steps, to_internal_fan(prev_fan),
           to_internal_fan(requested_fan));
  for (int i = 0; i < fan_steps; i++) {
    ESP_LOGD(TAG, "Sending fan toggle command");
    send_nec_command_(CMD_FAN_TOGGLE);
  }

  // Handle temperature setpoint (setpoint, not current temp!)
  int temp_diff = int(roundf(prev_temp)) - int(roundf(requested_temp));
  // Invert the sign as we otherwise move in the wrong direction
  temp_diff = -temp_diff;
  ESP_LOGD(TAG, "Temperature difference: %d", temp_diff);

  // There is a 6s busy/programming window after sending the initial setpoint command.
  // If we send again within this window the device will immediately accept it instead of waiting for another command.
  // This means we need to initially send 2 commands to set the initial temperature,
  // and then we can send 1 command at a time after that if we are still within the busy window.
  // We track the busy window with setpoint_busy_until_.
  uint32_t now = millis();
  if (temp_diff != 0) {
    if (now < this->setpoint_busy_until_) {
      ESP_LOGW(TAG, "Setpoint busy (until %u ms), sending only one step", this->setpoint_busy_until_);
      // Only send one step in the right direction
      if (temp_diff > 0) {
        ESP_LOGD(TAG, "Sending single temperature up command");
        send_nec_command_(CMD_TEMP_UP);
      } else {
        ESP_LOGD(TAG, "Sending single temperature down command");
        send_nec_command_(CMD_TEMP_DOWN);
      }
      // Do not update setpoint_busy_until_ (keep original busy window)
    } else {
      ESP_LOGD(TAG, "Setpoint not busy, sending full difference");
      if (temp_diff > 0) {
        ESP_LOGD(TAG, "Increasing temperature by %d steps", temp_diff);
        for (int i = 0; i < temp_diff + 1; i++) {
          ESP_LOGD(TAG, "Sending temperature up command");
          send_nec_command_(CMD_TEMP_UP);
        }
      } else {
        ESP_LOGD(TAG, "Decreasing temperature by %d steps", -temp_diff);
        for (int i = 0; i < -temp_diff + 1; i++) {
          ESP_LOGD(TAG, "Sending temperature down command");
          send_nec_command_(CMD_TEMP_DOWN);
        }
      }
      this->setpoint_busy_until_ = now + 6000;  // 6 seconds busy window
      ESP_LOGI(TAG, "Set setpoint busy until %u ms", this->setpoint_busy_until_);
    }
  }

  this->mode_before_ = this->mode;
  this->fan_mode_before_ = this->fan_mode.value();
  this->target_temperature_before_ = this->target_temperature;
  this->prev_dehumidify_ = requested_dehumidify;
  this->prev_on_off_ = (this->mode != climate::CLIMATE_MODE_OFF);
  ESP_LOGD(TAG, "New state: mode=%d, fan_mode=%d, target_temperature=%.1f, dehumidify=%s", to_internal_mode(this->mode),
           to_internal_fan(this->fan_mode.value()), this->target_temperature,
           this->prev_dehumidify_ ? "true" : "false");

  // Now publish state at the end
  this->publish_state();
  this->save_emerio_pac_state_();
}

void EmerioPac125152Climate::calibrate_state(climate::ClimateMode mode, float temperature,
                                             climate::ClimateFanMode fan_mode) {
  this->mode_before_ = mode;
  this->fan_mode_before_ = fan_mode;
  this->target_temperature_before_ = temperature;
  this->mode = mode;
  this->fan_mode = fan_mode;
  this->target_temperature = temperature;
  this->prev_on_off_ = (mode != climate::CLIMATE_MODE_OFF);
  ESP_LOGD(TAG, "Calibrating state: mode=%d, temperature=%.1f, fan_mode=%d", mode, temperature, fan_mode);
  this->publish_state();
  this->save_emerio_pac_state_();
}

void EmerioPac125152Climate::do_calibrate_state(int mode, float temperature, int fan_mode) {
  this->calibrate_state(static_cast<climate::ClimateMode>(mode), temperature,
                        static_cast<climate::ClimateFanMode>(fan_mode));
}
}  // namespace emerio_pac_125152
}  // namespace esphome
