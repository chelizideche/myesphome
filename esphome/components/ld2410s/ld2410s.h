#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif

#include "esphome/core/automation.h"
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#include <functional>
#include <iomanip>

namespace esphome {
namespace ld2410s {

static const uint16_t NO_SUB_CMD = 0xffff;
static const uint8_t CMD_EXEC_BUFFER_SIZE = 32;
static const size_t RCV_BUFFER_SIZE = 128;

struct TriggersT {
  uint32_t trigger[16];
  uint32_t hold[16];
  uint32_t snr[16];
  uint8_t selected_gate{0};
};

struct CmdFrameT {
  uint8_t data[128];
  uint32_t header;
  uint32_t footer;
  uint16_t length;
  uint16_t command;
  uint16_t data_length;
};

struct CmdAckT {
  uint8_t data[128];
  uint16_t command{0};
  uint16_t length{0};
  bool result{false};
};
enum class PackageType { SHORT_DATA_FRAME, STD_DATA_FRAME, CMD_FRAME, UNKNOWN, BAD_SIZE };

enum class CmdState { EMPTY, SCHEDULED, SENT };
struct CmdT {
  uint32_t time_started;
  uint8_t retry;
  CmdState state = CmdState::EMPTY;
  CmdFrameT *cmd_frame;
};

class LD2410SListener {
 public:
  virtual void on_presence(bool presence){};
  virtual void on_distance(uint16_t distance){};
  virtual void on_threshold_update(bool running){};
  virtual void on_threshold_progress(uint16_t progress){};
  virtual void on_fw_version(std::string &val){};
  virtual void on_trigger_threshold_ts(std::string &val){};
  virtual void on_trigger_hold_ts(std::string &val){};
  virtual void on_trigger_snr_ts(std::string &val){};
  virtual void on_energy_values_ts(std::string &val){};
};

class LD2410S : public uart::UARTDevice, public Component {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override;

  void register_listener(LD2410SListener *listener) { this->listeners_.push_back(listener); };

  void read_all();
  void write_all();
  void calibration();
  void factory_reset();
  void toggle_minimal();

  void set_delay(float delay);
  void set_distance_reporting_freq(float distance_reporting_freq);
  void set_max_distance(float max_distance);
  void set_min_distance(float min_distance);
  void set_status_reporting_freq(float status_reporting_freq);
  void set_trigger_selected_gate(float trigger_selected_gate);
  void set_trigger_threshold(float trigger_threshold);
  void set_trigger_hold(float trigger_hold);
  void set_trigger_snr(float trigger_snr);
  void set_response_speed_select(const std::string &response_speed_select);

#ifdef USE_NUMBER
  void set_max_distance_number(number::Number *max_distance_number) {
    this->max_distance_number_ = max_distance_number;
  };
  void set_min_distance_number(number::Number *min_distance_number) {
    this->min_distance_number_ = min_distance_number;
  };
  void set_no_delay_number(number::Number *delay_number) { this->no_delay_number_ = delay_number; };
  void set_status_reporting_freq_number(number::Number *status_reporting_freq_number) {
    this->status_reporting_freq_number_ = status_reporting_freq_number;
  };
  void set_distance_reporting_freq_number(number::Number *distance_reporting_freq_number) {
    this->distance_reporting_freq_number_ = distance_reporting_freq_number;
  };

  void set_trigger_threshold_number(number::Number *trigger_threshold_number) {
    this->trigger_threshold_number_ = trigger_threshold_number;
  };
  void set_trigger_hold_number(number::Number *trigger_hold_number) {
    this->trigger_hold_number_ = trigger_hold_number;
  };
  void set_trigger_snr_number(number::Number *trigger_snr_number) { this->trigger_snr_number_ = trigger_snr_number; };
  void set_trigger_selected_gate_number(number::Number *trigger_selected_gate_number) {
    this->trigger_selected_gate_number_ = trigger_selected_gate_number;
    this->trigger_selected_gate_number_->publish_state(this->triggers_.selected_gate);
    this->trigger_threshold_number_->publish_state(this->triggers_.trigger[this->triggers_.selected_gate]);
    this->trigger_hold_number_->publish_state(this->triggers_.hold[this->triggers_.selected_gate]);
    this->trigger_snr_number_->publish_state(this->triggers_.snr[this->triggers_.selected_gate]);
  };
#endif

#ifdef USE_BUTTON
  void set_read_all_button(button::Button *button) { this->read_all_button_ = button; };
  void set_write_all_button(button::Button *button) { this->write_all_button_ = button; };
  void set_calibration_button(button::Button *button) { this->calibration_button_ = button; };
  void set_factory_reset_button(button::Button *button) { this->factory_reset_button_ = button; };
  void set_toggle_minimal_output_button(button::Button *button) { this->toggle_minimal_output_button_ = button; };
#endif

#ifdef USE_SELECT
  void set_response_speed_select(select::Select *selector) { this->response_speed_select_ = selector; };
#endif

 private:
  size_t rcv_end_pos_ = 0;
  uint32_t max_dist_{0};
  uint32_t min_dist_{0};
  uint32_t delay_{0};
  uint32_t status_freq_{0};
  uint32_t dist_freq_{0};
  uint32_t resp_speed_{0};
  uint32_t energy_values_[16];
  uint16_t energy_values_count_ = 0;
  uint8_t rcv_buffer_[RCV_BUFFER_SIZE];
  uint8_t active_ = 0;
  uint8_t last_ = 0;
  bool minimal_output_{true};
  bool cmd_active_{false};

  std::string energy_values_str_ = "";
  std::vector<LD2410SListener *> listeners_{};
  CmdT commands_[CMD_EXEC_BUFFER_SIZE];
  TriggersT triggers_;

  void loop_send_command_();
  void cmd_buffer_insert_(CmdFrameT *cmd_frame);
  void cmd_buffer_finished_();
  void cmd_buffer_inc_(uint8_t &index);

#ifdef USE_NUMBER
  number::Number *max_distance_number_{nullptr};
  number::Number *min_distance_number_{nullptr};
  number::Number *no_delay_number_{nullptr};
  number::Number *status_reporting_freq_number_{nullptr};
  number::Number *distance_reporting_freq_number_{nullptr};

  number::Number *trigger_threshold_number_{nullptr};
  number::Number *trigger_hold_number_{nullptr};
  number::Number *trigger_snr_number_{nullptr};
  number::Number *trigger_selected_gate_number_{nullptr};
#endif

#ifdef USE_BUTTON
  button::Button *read_all_button_{nullptr};
  button::Button *write_all_button_{nullptr};
  button::Button *calibration_button_{nullptr};
  button::Button *factory_reset_button_{nullptr};
  button::Button *toggle_minimal_output_button_{nullptr};
#endif

#ifdef USE_SELECT
  select::Select *response_speed_select_{nullptr};
#endif

  void receive_();
  PackageType get_frame_type_(uint8_t *buffer, size_t pos);
  size_t get_frame_start_(uint8_t *buffer, size_t end_pos, PackageType type);
  size_t get_payload_size_(uint8_t *buffer, size_t end_pos, PackageType type, size_t start_pos);

  void process_short_data_frame_(uint8_t *data);
  void process_data_frame_(uint8_t *data, size_t data_size);
  void process_cmd_frame_(uint8_t *buffer, size_t len);

  CmdAckT parse_cms_frame_(uint8_t *buffer, size_t length);

  void schedule_cmd_(const char *msg, uint16_t command, uint16_t sub_command = NO_SUB_CMD);
  void schedule_cmd_frame_(uint16_t command, uint16_t sub_command = NO_SUB_CMD);
  void cmd_frame_append_data_(CmdFrameT *cmd_frame, const uint8_t *append_data, size_t append_data_size);
  void cmd_frame_append_data_(CmdFrameT *cmd_frame, const uint16_t *append_data, size_t append_data_size);
  void cmd_frame_append_data_(CmdFrameT *cmd_frame, const uint32_t *append_data, size_t append_data_size);

  void send_command_(CmdFrameT *cmd_frame);

  void process_ack_config_read_(uint8_t *data);
  void process_ack_fw_read_(const uint8_t *data);
  void process_ack_trigger_threshold_read_(uint8_t *data);
  void process_ack_trigger_hold_read_(uint8_t *data);
  void process_ack_trigger_snr_read_(uint8_t *data);
  void process_data_energy_values_read_(uint8_t *data);

  void update_ts_thresholds_();
  void update_ts_holds_();
  void update_ts_snrs_();
  void update_ts_energy_values_();

  std::string format_int_(uint32_t *in, uint8_t len, uint8_t min_w);
  void four_byte_to_int_array_(uint8_t *in, uint32_t *out, uint8_t out_len);
  void hex_diag_(const char *msg, const uint8_t *data, size_t length);
  int read_int_(const uint8_t *buffer, size_t pos, size_t len);
};

}  // namespace ld2410s
}  // namespace esphome
