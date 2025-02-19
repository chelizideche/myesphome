#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "dynamic_lamp.h"
#include <string>
#include <cstring>
#include <string_view>
#include <vector>
#include <array>
#include <list>
#include <optional>
#include <algorithm>
#include <cinttypes>
#include <bit>
#include <sstream>

namespace esphome {
namespace dynamic_lamp {

static const char *TAG = "dynamic_lamp";

void DynamicLampComponent::setup() {
  this->begin();
}

void DynamicLampComponent::begin() {
  for (uint8_t i=0; i < 16; i++) {
    this->active_lamps_[i] = CombinedLamp();
    this->active_lamps_[i].active = false;
    this->available_outputs_[i] = LinkedOutput{ false, false, "", 0, nullptr, 0.0, 0, 0, 0.0f, 1.0f, false };
  }
  this->restore_lamp_settings_();
  this->restore_timers_();
}

void DynamicLampComponent::loop() {
  uint8_t i = 0;
  for (i = 0; i < this->lamp_count_; i++) {
    if (this->active_lamps_[i].active == true && this->active_lamps_[i].update_ == true) {
      uint8_t j = 0;
      for (j = 0; j < 16; j++) {
        if (this->active_lamps_[i].used_outputs[j] == true) {
          // Update level
          float new_state;
          new_state = this->active_lamps_[i].state_;
          switch (this->available_outputs_[j].mode) {
            case MODE_EQUAL:
              if (this->available_outputs_[j].min_value && new_state < *this->available_outputs_[j].min_value) {
                new_state = *this->available_outputs_[j].min_value;
              }
              else if (this->available_outputs_[j].max_value && new_state > *this->available_outputs_[j].max_value) {
                new_state = *this->available_outputs_[j].max_value;
              }
              break;
            case MODE_STATIC:
              new_state = this->available_outputs_[j].mode_value;
              break;
            case MODE_PERCENTAGE:
              new_state = this->active_lamps_[i].state_ * this->available_outputs_[j].mode_value;
              if (this->available_outputs_[j].min_value && new_state < *this->available_outputs_[j].min_value) {
                new_state = *this->available_outputs_[j].min_value;
              }
              else if (this->available_outputs_[j].max_value && new_state > *this->available_outputs_[j].max_value) {
                new_state = *this->available_outputs_[j].max_value;
              }
              break;
            case MODE_FUNCTION:
              // ToDo - yet to be implemented
              ESP_LOGW(TAG, "Mode %d for output %s is not implemented yet, sorry", this->available_outputs_[j].mode, this->available_outputs_[j].output_id.c_str());
              this->status_set_warning();
              continue;
            default:
              // Unknown
              ESP_LOGW(TAG, "Unknown mode %d for output %s", this->available_outputs_[j].mode, this->available_outputs_[j].output_id.c_str());
              this->status_set_warning();
              continue;
          }
          ESP_LOGV(TAG, "Setting output %s to level %f", this->available_outputs_[j].output_id.c_str(), new_state);
          this->available_outputs_[j].output->set_level(new_state);
        }
      }
      this->active_lamps_[i].update_ = false;
    }
  }
}

void DynamicLampComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Dynamic Lamp feature loaded");
  switch(this->save_mode_) {
    case SAVE_MODE_NONE:
      ESP_LOGCONFIG(TAG, "Save mode set to NONE");
      break;
    case SAVE_MODE_LOCAL:
      ESP_LOGCONFIG(TAG, "Save mode set to LOCAL");
      break;
    case SAVE_MODE_FRAM:
      ESP_LOGCONFIG(TAG, "Save mode set to FRAM");
      break;
    default:
      ESP_LOGCONFIG(TAG, "Currently only NONE(0), LOCAL(1) & FRAM(2) save modes supported, ignoring value %" PRIu8 " and defaulting to NONE!", this->save_mode_);
      this->save_mode_ = 0;
  }
  for (uint8_t i = 0; i < 16; i++) {
    if (this->available_outputs_[i].available == true) {
      ESP_LOGCONFIG(TAG, "Using output with id %s as output number %" PRIu8 "", this->available_outputs_[i].output_id.c_str(), i);
    }
  }
}

void DynamicLampComponent::set_save_mode(uint8_t save_mode) {
  this->save_mode_ = save_mode;
}

void DynamicLampComponent::add_available_output(output::FloatOutput * output, std::string output_id) {
  uint8_t counter = 0;
  while (this->available_outputs_[counter].available == true) {
    counter++;
  }
  if (counter > 15) {
    ESP_LOGW(TAG, "No more outputs available, max 16 outputs supported!");
    this->status_set_warning();
    return;
  }
  this->available_outputs_[counter].available = true;
  this->available_outputs_[counter].output_id = output_id;
  this->available_outputs_[counter].output = output;
  this->available_outputs_[counter].output_index = counter;
  counter++;
}

void DynamicLampComponent::add_lamp(std::string name) {
  if (this->lamp_count_ < 15) {
    this->active_lamps_[this->lamp_count_].active = true;
    memcpy(this->active_lamps_[this->lamp_count_].name, name.c_str(), 16);
    this->active_lamps_[this->lamp_count_].validation_byte = 'L';
    this->active_lamps_[this->lamp_count_].lamp_index = this->lamp_count_;
    this->active_lamps_[this->lamp_count_].used_outputs[0] = 0;
    this->active_lamps_[this->lamp_count_].used_outputs[1] = 0;
    this->fram_->write((0x0000 + (this->lamp_count_ * 24)), reinterpret_cast<unsigned char *>(&this->active_lamps_[this->lamp_count_]), 24);
    this->lamp_count_++;
    ESP_LOGV(TAG, "Added new lamp %s, total lamps now %" PRIu8 "", name.c_str(), this->lamp_count_);
    return;
  }
  ESP_LOGW(TAG, "No more lamps available, max 16 lamps supported!");
  this->status_set_warning();
}

void DynamicLampComponent::remove_lamp(std::string lamp_name) {
  uint8_t i = 0;
  while (i < this->lamp_count_) {
    std::string str(this->active_lamps_[i].name, this->active_lamps_[i].name + sizeof this->active_lamps_[i].name / sizeof this->active_lamps_[i].name[0]);
    if (str == lamp_name) {
      for (uint8_t j = 0; j < 16; j++) {
        uint8_t k = 0;
        uint8_t l = j;
        if (j > 7) {
          k = 1;
          l = j - 8;
        }
        bool output_in_use = static_cast<bool>(this->active_lamps_[i].used_outputs[k] & (1 << l));
        if (output_in_use == true) {
          this->available_outputs_[j].in_use = false;
          this->active_lamps_[i].used_outputs[j] = false;
        }
      }
      uint16_t memaddress = 0 + (i * 24);
      this->fram_->write(memaddress, {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
      }, 24);
      this->active_lamps_[i].active = false;
      this->lamp_count_--;
      ESP_LOGV(TAG, "Removed lamp %s, total lamps now %" PRIu8 "", this->active_lamps_[i].name, this->lamp_count_);
      return;
    }
    i++;
  }
  this->status_set_warning();
  ESP_LOGW(TAG, "No lamp with name %s defined !", lamp_name.c_str());
}

void DynamicLampComponent::add_output_to_lamp(std::string lamp_name, LinkedOutput *output) {
  if (output->available == false) {
    ESP_LOGW(TAG, "Output %s is not available, ignoring!", output->output_id.c_str());
    this->status_set_warning();
    return;
  }
  if (output->in_use == true) {
    ESP_LOGW(TAG, "Output %s is already in use, ignoring!", output->output_id.c_str());
    this->status_set_warning();
    return;
  }
  uint8_t i = 0;
  while (i < 16) {
    std::string str(this->active_lamps_[i].name, this->active_lamps_[i].name + sizeof this->active_lamps_[i].name / sizeof this->active_lamps_[i].name[0]);
    if (str == lamp_name) {
      uint8_t j = 0;
      if (output->output_index > 7) {
        j = 1;
      }
      this->active_lamps_[i].used_outputs[j] |= 1 << (output->output_index % 8);
      output->in_use = true;
      this->fram_->write((0x0000 + (this->active_lamps_[i].lamp_index * 24)), reinterpret_cast<unsigned char *>(&this->active_lamps_[i]), 24);
      ESP_LOGV(TAG, "Added output %s to lamp %s", output->output_id.c_str(), lamp_name.c_str());
      return;
    }
    i++;
  }
  this->status_set_warning();
  ESP_LOGW(TAG, "No lamp with name %s defined !", lamp_name.c_str());
}

void DynamicLampComponent::remove_output_from_lamp(std::string lamp_name, LinkedOutput *output) {
  uint8_t i = 0;
  while (i < 16) {
    std::string str(this->active_lamps_[i].name, this->active_lamps_[i].name + sizeof this->active_lamps_[i].name / sizeof this->active_lamps_[i].name[0]);
    if (str == lamp_name) {
      uint8_t k = output->output_index;
      uint8_t j = 0;
      if (output->output_index > 7) {
        j = 1;
        k = output->output_index - 8;
      }
      this->active_lamps_[i].used_outputs[j] &= ~(1 << k);
      this->fram_->write((0x0000 + (i * 24)), reinterpret_cast<unsigned char *>(&this->active_lamps_[i]), 24);
      output->in_use = false;
      ESP_LOGV(TAG, "Removed output %s from lamp %s", output->output_id.c_str(), lamp_name.c_str());
      return;
    }
    i++;
  }
  this->status_set_warning();
  ESP_LOGW(TAG, "No lamp with name %s defined !", lamp_name.c_str());
}

std::array<bool, 16> DynamicLampComponent::get_lamp_outputs(uint8_t lamp_number) {
  std::array<bool, 16> bool_array;
  for (uint8_t i = 0; i < 16; i++) {
    uint8_t j = 0;
    uint8_t k = i;
    if (i > 7) {
      j = 1;
      k = i - 8;
    }
    bool_array[i] = static_cast<bool>(this->active_lamps_[lamp_number].used_outputs[j] & (1 << k));
  }
  return bool_array;
}

uint8_t DynamicLampComponent::get_lamp_index_by_name_(std::string lamp_name) {
  uint8_t i = 0;
  for (i = 0; i < this->lamp_count_; i++) {
    std::string str(this->active_lamps_[i].name, this->active_lamps_[i].name + sizeof this->active_lamps_[i].name / sizeof this->active_lamps_[i].name[0]);  
    if (str == lamp_name) {
      return i;
    }
  }
  this->status_set_warning();
  ESP_LOGW(TAG, "No lamp with name %s defined !", lamp_name.c_str());
  return 255;
}

std::array<bool, 16> DynamicLampComponent::get_lamp_outputs_by_name_(std::string lamp_name) {
  uint8_t lamp_index = this->get_lamp_index_by_name_(lamp_name);
  if (lamp_index == 255) {
    std::array<bool, 16> bool_array;
    return bool_array;
  }
  return this->get_lamp_outputs(lamp_index);
}

bool DynamicLampComponent::add_timer(std::string timer_desc, std::string lamp_list_str, bool timer_active, uint8_t action, uint8_t hour,
                                     uint8_t minute, bool monday, bool tuesday, bool wednesday, bool thursday,
                                     bool friday, bool saturday, bool sunday) {
  std::vector<bool> lamp_list = this->build_lamp_list_from_list_str_(lamp_list_str);
  DynamicLampTimer new_timer;
  strncpy(reinterpret_cast<char *>(new_timer.timer_desc), timer_desc.c_str(), 36);
  unsigned char lamp_list_bytes[2] = {0, 0};
  for (uint8_t i = 0; i < lamp_list.size(); i++) {
    if (lamp_list[i] == true && this->active_lamps_[i].active != true) {
      ESP_LOGW(TAG, "Ignoring lamp number %" PRIu8 " as there is no active lamp with that index!", i);
      continue;
    }
    if (lamp_list[i] == true) {
      lamp_list_bytes[i / 8] |= 1 << (i % 8);
    }
  }
  memcpy(new_timer.lamp_list, lamp_list_bytes, 2);
  new_timer.in_use = true;
  new_timer.validation_bytes[0] = 'V';
  new_timer.validation_bytes[1] = 'D';
  new_timer.validation_bytes[2] = 'L';
  new_timer.validation_bytes[3] = 'T';
  new_timer.active = timer_active;
  new_timer.action = action;
  new_timer.hour = hour;
  new_timer.minute = minute;
  new_timer.monday = monday;
  new_timer.tuesday = tuesday;
  new_timer.wednesday = wednesday;
  new_timer.thursday = thursday;
  new_timer.friday = friday;
  new_timer.saturday = saturday;
  new_timer.sunday = sunday;
  ESPTime now = this->rtc_->now();
  time_t begin_date = now.timestamp;
  now.increment_day();
  time_t end_date = now.timestamp;
  new_timer.begin_date = begin_date;
  new_timer.end_date = end_date;
  unsigned char* timer_as_bytes = static_cast<unsigned char*>(static_cast<void*>(&new_timer));
  ESP_LOGV(TAG, "Added new timer %s with lamp-list %s, active %d, action %d, hour %d, minute %d, monday %d, tuesday %d, wednesday %d, thursday %d, friday %d, saturday %d, sunday %d",
           new_timer.timer_desc, lamp_list_str.c_str(), new_timer.active, new_timer.action, new_timer.hour, new_timer.minute, new_timer.monday,
           new_timer.tuesday, new_timer.wednesday, new_timer.thursday, new_timer.friday, new_timer.saturday, new_timer.sunday);
  uint8_t save_slot;
  for (save_slot = 0; save_slot < 128; save_slot++) {
    if (this->timers_[save_slot].in_use != true) {
      break;
    }
  }
  if (save_slot == 128) {
    ESP_LOGW(TAG, "No more timer slots available, max 128 timers supported!");
    this->status_set_warning();
    return false;
  }
  this->timers_[save_slot] = new_timer;
  this->fram_->write((0x4000 + (save_slot * 64)), timer_as_bytes, 64);
  return true;
}

std::vector<bool> DynamicLampComponent::build_lamp_list_from_list_str_(std::string lamp_list_str) {
  std::vector<uint8_t> lamp_list_vector = this->split_to_int_vector_(lamp_list_str);
  std::vector<bool> lamp_list;
  while (lamp_list.size() < 16) {
    lamp_list.push_back(false);
  }
  if (lamp_list_vector.size() > 16) {
    ESP_LOGW(TAG, "Too many lamps in list, only 16 supported!");
    this->status_set_warning();
    return lamp_list;
  }
  for (uint8_t i = 0; i < lamp_list_vector.size(); i++) {
    uint8_t lamp_index = lamp_list_vector[i];
    if (lamp_index > 15) {
      ESP_LOGW(TAG, "Lamp index %" PRIu8 " is out of range, only [0-15] supported!", lamp_list_vector[i]);
      this->status_set_warning();
      return lamp_list;
    }
    lamp_list[lamp_index] = true;
  }
  return lamp_list;
}

void DynamicLampComponent::read_fram_timers_to_log() {
  DynamicLampTimer timer;
  for (uint8_t i = 0; i < 128; i++) {
    this->fram_->read((0x4000 + (i * 64)), reinterpret_cast<unsigned char *>(&timer), 64);
    if (timer.validation_bytes[0] == 'V' && timer.validation_bytes[1] == 'D' && timer.validation_bytes[2] == 'L' && timer.validation_bytes[3] == 'T' && timer.in_use == true) {
      std::string lamp_names_str = "";
      for (uint8_t j = 0; j < 16; j++) {
        uint8_t k = 0;
        uint8_t l = j;
        if (j > 7) {
          k = 1;
          l = j - 8;
        }
        //bool lamp_included = static_cast<bool>(timer.lamp_list[j / 8] & (1 << (j % 8)));
        bool lamp_included = static_cast<bool>(timer.lamp_list[k] & (1 << l));
        if (lamp_included == true && this->active_lamps_[j].active == true) {
          if (lamp_names_str.length() > 0) {
            lamp_names_str += ", ";
          }
          std::string str(this->active_lamps_[j].name, this->active_lamps_[j].name + sizeof this->active_lamps_[j].name / sizeof this->active_lamps_[j].name[0]);
          lamp_names_str += str;
        }
      }
      ESP_LOGV(TAG, "Timer %s found: [ active: %d, action: %d, hour: %d, minute: %d, monday: %d, tuesday: %d, wednesday: %d, thursday: %d, friday: %d, saturday: %d, sunday: %d ]",
        timer.timer_desc, timer.active, timer.action, timer.hour, timer.minute, timer.monday, timer.tuesday,
        timer.wednesday, timer.thursday, timer.friday, timer.saturday, timer.sunday);
      ESP_LOGV(TAG, "Timer active for lamps %s", lamp_names_str.c_str());
    }
  }
}

void DynamicLampComponent::read_initialized_timers_to_log() {
  DynamicLampTimer timer;
  for (uint8_t i = 0; i < 128; i++) {
    if (this->timers_[i].in_use == true) {
      timer = this->timers_[i];
      std::string lamp_names_str = "";
      for (uint8_t j = 0; j < 16; j++) {
        uint8_t k = 0;
        uint8_t l = j;
        if (j > 7) {
          k = 1;
          l = j - 8;
        }
        //bool lamp_included = static_cast<bool>(timer.lamp_list[j / 8] & (1 << (j % 8)));
        bool lamp_included = static_cast<bool>(timer.lamp_list[k] & (1 << l));
        if (lamp_included == true && this->active_lamps_[j].active == true) {
          if (lamp_names_str.length() > 0) {
            lamp_names_str += ", ";
          }
          std::string str(this->active_lamps_[j].name, this->active_lamps_[j].name + sizeof this->active_lamps_[j].name / sizeof this->active_lamps_[j].name[0]);
          lamp_names_str += str;
        }
      }
      ESP_LOGV(TAG, "Timer %s found: [ active: %d, action: %d, hour: %d, minute: %d, monday: %d, tuesday: %d, wednesday: %d, thursday: %d, friday: %d, saturday: %d, sunday: %d ]",
        timer.timer_desc, timer.active, timer.action, timer.hour, timer.minute, timer.monday, timer.tuesday,
        timer.wednesday, timer.thursday, timer.friday, timer.saturday, timer.sunday);
      ESP_LOGV(TAG, "Timer active for lamps %s", lamp_names_str.c_str());
    }
  }
}

bool DynamicLampComponent::write_state_(uint8_t lamp_number, float state) {
  if (this->active_lamps_[lamp_number].active == true) {
    this->active_lamps_[lamp_number].state_ = state;
    this->active_lamps_[lamp_number].update_ = true;
    return true;
  }
  return false;
}

std::string DynamicLampComponent::get_lamp_name(uint8_t lamp_number) {
  std::string str(this->active_lamps_[lamp_number].name, this->active_lamps_[lamp_number].name + sizeof this->active_lamps_[lamp_number].name / sizeof this->active_lamps_[lamp_number].name[0]);
  return str;
}

void DynamicLampComponent::set_lamp_values_(uint8_t lamp_number, bool active, uint16_t selected_outputs, uint8_t mode, uint8_t mode_value) {

}

void DynamicLampComponent::restore_lamp_settings_() {
  switch (this->save_mode_) {
    case SAVE_MODE_LOCAL:
      // ToDo - yet to be implemented
      ESP_LOGW(TAG, "Save mode LOCAL not implemented yet, sorry");
      this->status_set_warning();
      break;
    case SAVE_MODE_FRAM:
      CombinedLamp lamp;
      for (uint8_t i=0; i < 16; i++) {
        this->fram_->read((0x0000 + (i * 24)), reinterpret_cast<unsigned char *>(&lamp), 24);
        if (lamp.validation_byte == 'L' && lamp.active == true) {
          this->active_lamps_[i] = lamp;
          for (uint8_t j = 0; j < 16; j++) {
            uint8_t k = 0;
            uint8_t l = j;
            if (j > 7) {
              k = 1;
              l = j - 8;
            }
            bool output_in_use = static_cast<bool>(lamp.used_outputs[k] & (1 << l));
            if (output_in_use == true) {
              this->available_outputs_[j].in_use = true;
            }
          }
        } else {
          lamp = CombinedLamp();
          this->active_lamps_[i] = lamp;
          this->active_lamps_[i].active = false;
        }
      }
      break;
  }
}

void DynamicLampComponent::restore_timers_() {
  DynamicLampTimer timer;
  switch (this->save_mode_) {
    case SupportedSaveModes::SAVE_MODE_NONE:
      for (uint8_t i = 0; i < 128; i++) {
        timer = DynamicLampTimer();
        this->timers_[i] = timer;
        this->timers_[i].in_use = false;
      }
      break;
    case SupportedSaveModes::SAVE_MODE_LOCAL:
      // ToDo - yet to be implemented
      ESP_LOGW(TAG, "Save mode LOCAL not implemented yet, sorry");
      this->status_set_warning();
      break;
    case SupportedSaveModes::SAVE_MODE_FRAM:
      std::string lamp_names_str;
      for (uint8_t i = 0; i < 128; i++) {
        this->fram_->read((0x4000 + (i * 64)), reinterpret_cast<unsigned char *>(&timer), 64);
        if (timer.validation_bytes[0] == 'V' && timer.validation_bytes[1] == 'D' && timer.validation_bytes[2] == 'L' && timer.validation_bytes[3] == 'T' && timer.in_use == true) {
          this->timers_[i] = timer;
        } else {
          timer  = DynamicLampTimer();
          this->timers_[i] = timer;
          this->timers_[i].in_use = false;
        }
      }
      break;
  }
}

void DynamicLampComponent::clear_fram() {
  this->fram_->clear();
  ESP_LOGV(TAG, "Cleared FRAM");
}

std::vector<uint8_t> DynamicLampComponent::split_to_int_vector_(std::string lamp_list_str) {
  std::vector<uint8_t> tokens;
  std::stringstream sstream(lamp_list_str);
  std::string segment;
  while(std::getline(sstream, segment, ',')) {
    tokens.push_back(static_cast<uint8_t>(atoi(segment.c_str())));
  }
  return tokens;
}

}  // namespace dynamic_lamp
}  // namespace esphome
