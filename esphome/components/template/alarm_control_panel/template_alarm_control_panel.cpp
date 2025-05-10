
#include "template_alarm_control_panel.h"
#include <utility>
#include "esphome/components/alarm_control_panel/alarm_control_panel.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

using namespace esphome::alarm_control_panel;

static const char *const TAG = "template.alarm_control_panel";

TemplateAlarmControlPanel::TemplateAlarmControlPanel(){};

#ifdef USE_BINARY_SENSOR
void TemplateAlarmControlPanel::add_sensor(binary_sensor::BinarySensor *sensor, uint16_t flags, AlarmSensorType type) {
  this->sensor_map_[sensor].flags = flags;
  this->sensor_map_[sensor].type = type;
};
#endif

void TemplateAlarmControlPanel::dump_config() {
  ESP_LOGCONFIG(TAG, "TemplateAlarmControlPanel:");
  ESP_LOGCONFIG(TAG, "  Current State: %s", LOG_STR_ARG(alarm_control_panel_state_to_string(this->current_state_)));
  ESP_LOGCONFIG(TAG, "  Number of Codes: %u", this->codes_.size());
  if (!this->codes_.empty())
    ESP_LOGCONFIG(TAG, "  Requires Code To Arm: %s", YESNO(this->requires_code_to_arm_));
  ESP_LOGCONFIG(TAG, "  Arming Away Time: %" PRIu32 "s", (this->arming_away_time_ / 1000));
  if (this->arming_home_time_ != 0)
    ESP_LOGCONFIG(TAG, "  Arming Home Time: %" PRIu32 "s", (this->arming_home_time_ / 1000));
  if (this->arming_night_time_ != 0)
    ESP_LOGCONFIG(TAG, "  Arming Night Time: %" PRIu32 "s", (this->arming_night_time_ / 1000));
  ESP_LOGCONFIG(TAG, "  Pending Time: %" PRIu32 "s", (this->pending_time_ / 1000));
  ESP_LOGCONFIG(TAG, "  Trigger Time: %" PRIu32 "s", (this->trigger_time_ / 1000));
  ESP_LOGCONFIG(TAG, "  Supported Features: %" PRIu32, this->get_supported_features());
#ifdef USE_BINARY_SENSOR
  for (const auto &sensor_info : this->sensor_map_) {
    auto *sensor = sensor_info.first;
    auto &info = sensor_info.second;
    ESP_LOGCONFIG(TAG, "  Binary Sensor:");
    ESP_LOGCONFIG(TAG, "    Name: %s", sensor->get_name().c_str());
    ESP_LOGCONFIG(TAG, "    Armed home bypass: %s", TRUEFALSE(info.flags & BINARY_SENSOR_MODE_BYPASS_ARMED_HOME));
    ESP_LOGCONFIG(TAG, "    Armed night bypass: %s", TRUEFALSE(info.flags & BINARY_SENSOR_MODE_BYPASS_ARMED_NIGHT));
    ESP_LOGCONFIG(TAG, "    Auto bypass: %s", TRUEFALSE(info.flags & BINARY_SENSOR_MODE_BYPASS_AUTO));
    ESP_LOGCONFIG(TAG, "    Chime mode: %s", TRUEFALSE(info.flags & BINARY_SENSOR_MODE_CHIME));
    const char *sensor_type;
    switch (info.type) {
      case ALARM_SENSOR_TYPE_INSTANT:
        sensor_type = "instant";
        break;
      case ALARM_SENSOR_TYPE_DELAYED_FOLLOWER:
        sensor_type = "delayed_follower";
        break;
      case ALARM_SENSOR_TYPE_INSTANT_ALWAYS:
        sensor_type = "instant_always";
        break;
      case ALARM_SENSOR_TYPE_DELAYED:
      default:
        sensor_type = "delayed";
    }
    ESP_LOGCONFIG(TAG, "    Sensor type: %s", sensor_type);
  }
#endif
}

void TemplateAlarmControlPanel::setup() {
  ESP_LOGCONFIG(TAG, "Running setup for '%s'", this->name_.c_str());
  this->current_state_ = ACP_STATE_DISARMED;
  if (this->restore_mode_ == ALARM_CONTROL_PANEL_RESTORE_DEFAULT_DISARMED) {
    uint8_t value;
    this->pref_ = global_preferences->make_preference<uint8_t>(this->get_object_id_hash());
    if (this->pref_.load(&value)) {
      this->current_state_ = static_cast<alarm_control_panel::AlarmControlPanelState>(value);
    }
  }
  this->desired_state_ = this->current_state_;
}

void TemplateAlarmControlPanel::loop() {
  // change from ARMING to ARMED_x after the arming_time_ has passed
  if (this->current_state_ == ACP_STATE_ARMING) {
    if ((millis() - this->last_update_) > this->desired_arming_delay_()) {
      this->auto_bypass_sensors_();
      this->publish_state(this->desired_state_);
    }
    return;
  }
  // change from PENDING to TRIGGERED after the delay_time_ has passed
  if (this->current_state_ == ACP_STATE_PENDING && (millis() - this->last_update_) > this->pending_time_) {
    this->publish_state(ACP_STATE_TRIGGERED);
    return;
  }
  auto next_state = this->current_state_;
  // reset triggered if all clear
  if (this->current_state_ == ACP_STATE_TRIGGERED && this->trigger_time_ > 0 &&
      (millis() - this->last_update_) > this->trigger_time_) {
    next_state = this->desired_state_;
  }

#ifdef USE_BINARY_SENSOR
  bool delayed_sensor_faulted = false;
  bool instant_sensor_faulted = false;

  // Test all of the sensors regardless of the alarm panel state
  for (auto &sensor_info : this->sensor_map_) {
    auto *sensor = sensor_info.first;
    auto &info = sensor_info.second;
    // Check for chime zones
    if (info.flags & BINARY_SENSOR_MODE_CHIME) {
      // Look for the transition from closed to open
      if ((!info.chime_active) && (sensor->state)) {
        // Must be disarmed to chime
        if (this->current_state_ == ACP_STATE_DISARMED) {
          this->chime_callback_.call();
        }
      }
      // Record the sensor state change
      info.chime_active = sensor_info.first->state;
    }
    // Check for faulted sensors
    if (sensor->state) {
      // Skip if auto bypassed
      if (info.auto_bypassed) {
        continue;
      }
      // Skip if bypass armed home
      if ((this->desired_state_ == ACP_STATE_ARMED_HOME) && (info.flags & BINARY_SENSOR_MODE_BYPASS_ARMED_HOME)) {
        continue;
      }
      // Skip if bypass armed night
      if ((this->desired_state_ == ACP_STATE_ARMED_NIGHT) && (info.flags & BINARY_SENSOR_MODE_BYPASS_ARMED_NIGHT)) {
        continue;
      }

      switch (info.type) {
        case ALARM_SENSOR_TYPE_INSTANT_ALWAYS:
          next_state = ACP_STATE_TRIGGERED;
          [[fallthrough]];
        case ALARM_SENSOR_TYPE_INSTANT:
          instant_sensor_faulted = true;
          break;
        case ALARM_SENSOR_TYPE_DELAYED_FOLLOWER:
          if (this->is_state_armed(next_state)) {
            instant_sensor_faulted = true;
          } else {
            delayed_sensor_faulted = true;
          }
          break;
        case ALARM_SENSOR_TYPE_DELAYED:
        default:
          delayed_sensor_faulted = true;
      }
    }
  }
  // Update all sensors ready flag
  bool sensors_ready = !(instant_sensor_faulted || delayed_sensor_faulted);

  // Call the ready state change callback if there was a change
  if (this->sensors_ready_ != sensors_ready) {
    this->sensors_ready_ = sensors_ready;
    this->ready_callback_.call();
  }

  // Update next_state based on faulted sensors.
  if (instant_sensor_faulted && (next_state != ACP_STATE_DISARMED)) {
    next_state = ACP_STATE_TRIGGERED;
  } else if (delayed_sensor_faulted && this->is_state_armed(next_state)) {
    next_state = (this->pending_time_ > 0) ? ACP_STATE_PENDING : ACP_STATE_TRIGGERED;
  }
#endif
  if (next_state != this->current_state_) {
    this->publish_state(next_state);
  }
}

bool TemplateAlarmControlPanel::is_code_valid_(const optional<std::string> &code) {
  if (this->codes_.empty()) {
    return true;
  }
  if (!code.has_value()) {
    ESP_LOGD(TAG, "No code provided");
    return false;
  }
  ESP_LOGVV(TAG, "Checking code: %s", code.value().c_str());
  return (this->codes_.count(code.value()) == 1);
}

uint32_t TemplateAlarmControlPanel::get_supported_features() const {
  uint32_t features = ACP_FEAT_ARM_AWAY | ACP_FEAT_TRIGGER;
  if (this->supports_arm_home_) {
    features |= ACP_FEAT_ARM_HOME;
  }
  if (this->supports_arm_night_) {
    features |= ACP_FEAT_ARM_NIGHT;
  }
  return features;
}

void TemplateAlarmControlPanel::arm_(const optional<std::string> &code,
                                     alarm_control_panel::AlarmControlPanelState state) {
  if (this->current_state_ != ACP_STATE_DISARMED) {
    ESP_LOGW(TAG, "Cannot arm when not disarmed");
    return;
  }
  if (this->requires_code_to_arm_ && !this->is_code_valid_(code)) {
    ESP_LOGW(TAG, "Not arming - code doesn't match");
    return;
  }
  this->desired_state_ = state;
  auto next_state = (this->desired_arming_delay_() > 0) ? ACP_STATE_ARMING : state;
  this->publish_state(next_state);
}

void TemplateAlarmControlPanel::auto_bypass_sensors_() {
#ifdef USE_BINARY_SENSOR
  // Check for faulted bypass_auto sensors and remove them from monitoring
  for (auto &sensor_info : this->sensor_map_) {
    auto *sensor = sensor_info.first;
    auto &info = sensor_info.second;
    if ((info.flags & BINARY_SENSOR_MODE_BYPASS_AUTO) && (sensor->state)) {
      ESP_LOGI(TAG, "%s is faulted and will be automatically bypassed", sensor->get_name().c_str());
      info.auto_bypassed = true;
    }
  }
#endif
}

void TemplateAlarmControlPanel::clear_auto_bypassed_sensors_() {
#ifdef USE_BINARY_SENSOR
  for (auto &sensor_info : this->sensor_map_) {
    auto &info = sensor_info.second;
    info.auto_bypassed = false;
  }
#endif
}

uint32_t TemplateAlarmControlPanel::desired_arming_delay_() {
  switch (this->desired_state_) {
    case ACP_STATE_ARMED_AWAY:
      return this->arming_away_time_;
    case ACP_STATE_ARMED_HOME:
      return this->arming_home_time_;
    case ACP_STATE_ARMED_NIGHT:
      return this->arming_night_time_;
    default:
      return 0;
  }
}

void TemplateAlarmControlPanel::control(const AlarmControlPanelCall &call) {
  if (!call.get_state()) {
    ESP_LOGE(TAG, "No state specified");
    return;
  }
  auto state = *call.get_state();
  switch (state) {
    case ACP_STATE_ARMED_AWAY:
    case ACP_STATE_ARMED_HOME:
    case ACP_STATE_ARMED_NIGHT:
      this->arm_(call.get_code(), state);
      break;
    case ACP_STATE_DISARMED:
      if (!this->is_code_valid_(call.get_code())) {
        ESP_LOGW(TAG, "Not disarming - code doesn't match");
        return;
      }
      this->desired_state_ = ACP_STATE_DISARMED;
      this->clear_auto_bypassed_sensors_();
      [[fallthrough]];
    case ACP_STATE_PENDING:
    case ACP_STATE_TRIGGERED:
      this->publish_state(state);
      break;
    default:
      ESP_LOGE(TAG, "Transition to %s not yet implemented", LOG_STR_ARG(alarm_control_panel_state_to_string(state)));
  }
}

}  // namespace template_
}  // namespace esphome
