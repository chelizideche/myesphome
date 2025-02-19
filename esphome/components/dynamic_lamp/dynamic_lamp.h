#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/optional.h"
#include "esphome/core/log.h"
#include "esphome/core/time.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/fram/FRAM.h"

namespace esphome {
namespace dynamic_lamp {

enum SupportedSaveModes : uint8_t {
  SAVE_MODE_NONE = 0,
  SAVE_MODE_LOCAL = 1,
  SAVE_MODE_FRAM = 2
};

enum LinkedOutputModeIdx : uint8_t {
  MODE_EQUAL = 0,
  MODE_STATIC = 1,
  MODE_PERCENTAGE = 2,
  MODE_FUNCTION = 3
};

struct LinkedOutput {
  bool available = false;
  bool in_use = false;
  std::string output_id;
  uint8_t output_index;
  output::FloatOutput *output;
  float state;
  uint8_t mode = 0;
  float mode_value = 0;
  optional<float> min_value;
  optional<float> max_value;
  bool update_level = false;
};

enum DynamicLampIdx : uint8_t {
  LAMP_1 = 0,
  LAMP_2 = 1,
  LAMP_3 = 2,
  LAMP_4 = 3,
  LAMP_5 = 4,
  LAMP_6 = 5,
  LAMP_7 = 6,
  LAMP_8 = 7,
  LAMP_9 = 8,
  LAMP_10 = 9,
  LAMP_11 = 10,
  LAMP_12 = 11,
  LAMP_13 = 12,
  LAMP_14 = 13,
  LAMP_15 = 14,
  LAMP_16 = 15,
};

struct CombinedLamp {
  uint8_t lamp_index;
  bool active = false;
  std::string name = "";
  bool update_ = false;
  float state_;
  bool used_outputs[16];
};

struct DynamicLampTimer {
  unsigned char validation_bytes[4];
  unsigned char timer_desc[36];
  unsigned char lamp_list[2];
  bool in_use : 1;
  uint8_t action : 3;
  uint8_t hour : 5;
  uint8_t minute : 6;
  bool active : 1;
  bool monday : 1;
  bool tuesday : 1;
  bool wednesday : 1;
  bool thursday : 1;
  bool friday : 1;
  bool saturday : 1;
  bool sunday : 1;
  bool respect_dst : 1;
  time_t begin_date : 64;
  time_t end_date : 64;
};

class DynamicLamp;

class DynamicLampComponent : public Component {
 public:
  explicit DynamicLampComponent(time::RealTimeClock *rtc, fram::FRAM *fram) : rtc_(rtc), fram_(fram) {}
  void setup() override;
  void loop() override;
  void dump_config() override;
  void begin();
  void add_available_output(output::FloatOutput * output, std::string output_id);
  void set_save_mode(uint8_t save_mode);
  void add_lamp(std::string name);
  void remove_lamp(std::string name);
  std::string get_lamp_name(uint8_t lamp_number);
  void add_output_to_lamp(std::string lamp_name, LinkedOutput *output);
  void remove_output_from_lamp(std::string lamp_name, LinkedOutput *output);
  std::array<bool, 16> get_lamp_outputs(uint8_t lamp_number);
  bool add_timer(std::string timer_desc, std::string lamp_name, bool timer_active, uint8_t mode, uint8_t hour,
    uint8_t minute, bool monday, bool tuesday, bool wednesday, bool thursday,
    bool friday, bool saturday, bool sunday);
  void read_initialized_timers_to_log();
  void read_fram_timers_to_log();
  void clear_fram();

 protected:
  friend class DynamicLamp;
  time::RealTimeClock *rtc_;
  fram::FRAM *fram_;
  void restore_lamp_settings_();
  void restore_timers_();
  void set_lamp_values_(uint8_t lamp_number, bool active, uint16_t selected_outputs, uint8_t mode, uint8_t mode_value);
  bool write_state_(uint8_t lamp_number, float state);
  uint8_t get_lamp_index_by_name_(std::string lamp_name);
  std::array<bool, 16> get_lamp_outputs_by_name_(std::string lamp_name);
  std::vector<uint8_t> split_to_int_vector_(std::string lamp_list_str);
  std::vector<bool> build_lamp_list_from_list_str_(std::string lamp_list_str);
 
  CombinedLamp active_lamps_[16];
  LinkedOutput available_outputs_[16];
  DynamicLampTimer timers_[64];
  uint8_t save_mode_;
  uint8_t lamp_count_ = 0;
};


}  // namespace dynamic_lamp
}  // namespace esphome
