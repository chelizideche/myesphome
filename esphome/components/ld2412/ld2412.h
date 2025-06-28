#pragma once
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ld2412 {

class LD2412Component : public Component, public uart::UARTDevice {
#ifdef USE_SENSOR
  SUB_SENSOR(moving_target_distance)
  SUB_SENSOR(still_target_distance)
  SUB_SENSOR(moving_target_energy)
  SUB_SENSOR(still_target_energy)
  SUB_SENSOR(light)
  SUB_SENSOR(detection_distance)
#endif
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(target)
  SUB_BINARY_SENSOR(moving_target)
  SUB_BINARY_SENSOR(still_target)
  SUB_BINARY_SENSOR(out_pin_presence_status)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(version)
  SUB_TEXT_SENSOR(mac)
#endif
#ifdef USE_SELECT
  SUB_SELECT(distance_resolution)
  SUB_SELECT(baud_rate)
  SUB_SELECT(light_function)
  SUB_SELECT(out_pin_level)
  SUB_SELECT(mode)
#endif
#ifdef USE_SWITCH
  SUB_SWITCH(engineering_mode)
  SUB_SWITCH(bluetooth)
#endif
#ifdef USE_BUTTON
  SUB_BUTTON(reset)
  SUB_BUTTON(restart)
  SUB_BUTTON(query)
#endif
#ifdef USE_NUMBER
  SUB_NUMBER(max_distance_gate)
  SUB_NUMBER(min_distance_gate)
  SUB_NUMBER(timeout)
  SUB_NUMBER(light_threshold)
#endif

 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  void set_light_out_control();
#ifdef USE_NUMBER
  void set_gate_still_threshold_number(int gate, number::Number *n);
  void set_gate_move_threshold_number(int gate, number::Number *n);
  void set_basic_config();
  void set_gate_threshold();
  void get_gate_threshold();
#endif
#if !defined(USE_NUMBER) && defined(USE_SELECT)
  void set_basic_config();
#endif
#ifdef USE_SENSOR
  void set_gate_move_sensor(int gate, sensor::Sensor *s);
  void set_gate_still_sensor(int gate, sensor::Sensor *s);
#endif
  void set_throttle(uint16_t value) { this->throttle_ = value; };
  void set_bluetooth_password(const std::string &password);
  void set_engineering_mode(bool enable);
  void set_mode(const std::string &state);
  void read_all_info();
  void restart_and_read_all_info();
  void set_bluetooth(bool enable);
  void set_distance_resolution(const std::string &state);
  void set_baud_rate(const std::string &state);
  void factory_reset();

 protected:
  void send_command_(uint8_t command_str, const uint8_t *command_value, int command_value_len);
  void set_config_mode_(bool enable);
  void handle_periodic_data_(uint8_t *buffer, int len);
  bool handle_ack_data_(uint8_t *buffer, int len);
  void readline_(int readch, uint8_t *buffer, int len);
  void query_parameters_();
  void get_version_();
  void get_mac_();
  void get_distance_resolution_();
  void get_light_control_();
  void restart_();
  void query_dynamic_background_correction_();

  float light_threshold_ = -1;
  int32_t last_periodic_millis_ = 0;
  int32_t last_engineering_mode_change_millis_ = 0;
  uint16_t throttle_;
  bool dynamic_background_correction_active_;
  std::string version_;
  std::string mac_;
  std::string out_pin_level_;
  std::string light_function_;
#ifdef USE_NUMBER
  std::vector<number::Number *> gate_still_threshold_numbers_ = std::vector<number::Number *>(14);
  std::vector<number::Number *> gate_move_threshold_numbers_ = std::vector<number::Number *>(14);
#endif
#ifdef USE_SENSOR
  std::vector<sensor::Sensor *> gate_still_sensors_ = std::vector<sensor::Sensor *>(14);
  std::vector<sensor::Sensor *> gate_move_sensors_ = std::vector<sensor::Sensor *>(14);
#endif
};

}  // namespace ld2412
}  // namespace esphome
