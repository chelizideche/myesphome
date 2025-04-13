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
#include <map>
#include <sstream>

namespace esphome {
namespace ld2410s {

static const char *const TAG = "ld2410s";

static const uint8_t SHORT_DATA_FRAME_HEADER = 0x6E;
static const uint8_t SHORT_DATA_FRAME_FOOTER = 0x62;

static const uint32_t STD_DATA_FRAME_HEADER = 0xF1F2F3F4;
static const uint32_t STD_DATA_FRAME_FOOTER = 0xF5F6F7F8;

static const uint32_t CMD_FRAME_HEADER = 0xFAFBFCFD;
static const uint32_t CMD_FRAME_FOOTER = 0x01020304;

static const uint16_t CONFIG_MODE_START_CMD = 0x00FF;
static const uint16_t CONFIG_MODE_START_REPLY = 0x01FF;
static const uint8_t CONFIG_MODE_START_VALUE[] = {0x01, 0x00};

static const uint16_t CONFIG_MODE_END_CMD = 0x00FE;
static const uint16_t CONFIG_MODE_END_REPLY = 0x01FE;

static const uint16_t OUTPUT_MODE_SWITCH_CMD = 0x007A;
static const uint16_t OUTPUT_MODE_SWITCH_REPLY = 0x017A;
static const uint8_t OUTPUT_MODE_VALUE_STD[] = {0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
static const uint8_t OUTPUT_MODE_VALUE_MIN[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const uint16_t FW_READ_CMD = 0x0000;
static const uint16_t FW_READ_REPLY = 0x0100;

static const uint16_t SN_READ_CMD = 0x0011;
static const uint16_t SN_READ_REPLY = 0x0111;
static const uint16_t SN_WRITE_CMD = 0x0010;
static const uint16_t SN_WRITE_REPLY = 0x0110;

static const uint16_t PARAMS_READ_CMD = 0x0071;
static const uint16_t PARAMS_READ_REPLY = 0x0171;
static const uint16_t PARAMS_WRITE_CMD = 0x0070;
static const uint16_t PARAMS_WRITE_REPLY = 0x0170;
static const uint16_t CFG_MAX_DETECTION_VALUE = 0x0005;
static const uint16_t CFG_MIN_DETECTION_VALUE = 0x000A;
static const uint16_t CFG_NO_DELAY_VALUE = 0x0006;
static const uint16_t CFG_STATUS_FREQ_VALUE = 0x0002;
static const uint16_t CFG_DISTANCE_FREQ_VALUE = 0x000C;
static const uint16_t CFG_RESPONSE_SPEED_VALUE = 0x000B;
static const std::string RESPONSE_SPEED_NORMAL = "Normal";
static const std::string RESPONSE_SPEED_FAST = "Fast";

static const uint16_t CALIBRATION_CMD = 0x0009;
static const uint16_t CALIBRATION_TRIGGER_VALUE = 0x0002;
static const uint16_t CALIBRATION_RETENTION_VALUE = 0x0001;
static const uint16_t CALIBRATION_TIME_VALUE = 0x0078;  // 0x0078 // 0x001E

static const uint16_t GATE_TRIGGER_THRESHOLD_READ_CMD = 0x0073;
static const uint16_t GATE_TRIGGER_THRESHOLD_READ_REPLY = 0x0173;
static const uint16_t GATE_TRIGGER_THRESHOLD_WRITE_CMD = 0x0072;
static const uint16_t GATE_TRIGGER_THRESHOLD_WRITE_REPLY = 0x0172;
static const uint32_t GATE_TRIGGER_THRESHOLD_WRITE_DATA[] = {

    48, 42, 36, 34, 32, 31, 31, 31, 31,
    31, 31, 31, 31, 31, 31, 31
    // 10~95 dB

    // Factory defaults:
    // tool - reset
    //  https://github.com/MrUndead1996/ld2410s-esphome/issues/4
    //   48,42,36,34,32,31,31,31,31,31,31,31,31,31,31,31
    // tool default
    //  https://drive.google.com/drive/folders/1wC8KC-DaNavNbpeVouZ1HdiBzZ9YrAcg
    //   50,46,34,32,32,32,32,32,25,25,25,25,25,25,25,25
    //
    // semi good
    //   95,63,38,35,35,34,33,33,33,33,33,32,32,31,31,31
};

static const uint16_t GATE_HOLD_THRESHOLD_READ_CMD = 0x0077;
static const uint16_t GATE_HOLD_THRESHOLD_READ_REPLY = 0x0177;
static const uint16_t GATE_HOLD_THRESHOLD_WRITE_CMD = 0x0076;
static const uint16_t GATE_HOLD_THRESHOLD_WRITE_REPLY = 0x0176;
static const uint32_t GATE_HOLD_THRESHOLD_WRITE_DATA[] = {

    45, 42, 33, 32, 28, 28, 28, 28, 28,
    28, 28, 28, 28, 28, 28, 28
    // 10~95 dB

    // Factory defaults:
    // tool - reset
    //  https://github.com/MrUndead1996/ld2410s-esphome/issues/4
    //   45,42,33,32,28,28,28,28,28,28,28,28,28,28,28,28
    // tool default
    //   52,49,26,25,25,21,22,24,23,22,21,21,20,21,21,20
    //
    // semi good
    //   90,60,36,33,33,33,32,32,32,32,32,31,31,30,30,30
};

static const uint16_t GATE_SNR_READ_CMD = 0x0075;
static const uint16_t GATE_SNR_READ_REPLY = 0x0175;
static const uint16_t GATE_SNR_WRITE_CMD = 0x0074;
static const uint16_t GATE_SNR_WRITE_REPLY = 0x0174;
static const uint32_t GATE_SNR_WRITE_DATA[] = {

    34, 34, 34, 34, 34, 34, 34, 34, 34,
    34, 34, 34, 34, 34, 34, 34
    // 5~63 dB

    //
    // semi good
    //   35,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33
};

static const uint32_t CMD_EXEC_TIMEOUT = 1000;
static const uint8_t CMD_EXEC_REPEAT = 3;
static const uint8_t CMD_EXEC_BUFFER_SIZE = 32;

static const uint16_t NO_SUB_CMD = 0xffff;

struct TriggersT {
  uint8_t selected_gate{0};
  uint32_t trigger[16];
  uint32_t hold[16];
  uint32_t snr[16];
};

struct CmdFrameT {
  uint32_t header;
  uint16_t length;
  uint16_t command;
  uint8_t data[128];
  uint16_t data_length;
  uint32_t footer;
};

struct CmdAckT {
  uint16_t command{0};
  uint8_t data[128];
  uint16_t length{0};
  bool result{false};
};
enum class PackageType { SHORT_DATA_FRAME, STD_DATA_FRAME, CMD_FRAME, UNKNOWN, BAD_SIZE };

enum class CmdState { EMPTY, SCHEDULED, SENT };
struct CmdT {
  CmdState state = CmdState::EMPTY;
  CmdFrameT *cmd_frame;
  uint32_t time_started;
  uint8_t retry;
};

class LD2410SListener {
 public:
  virtual void on_presence(bool presence){};
  virtual void on_distance(int distance){};
  virtual void on_threshold_update(bool running){};
  virtual void on_threshold_progress(int progress){};
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
#ifdef USE_NUMBER
    this->trigger_selected_gate_number_->publish_state(this->triggers_.selected_gate);
    this->trigger_threshold_number_->publish_state(this->triggers_.trigger[this->triggers_.selected_gate]);
    this->trigger_hold_number_->publish_state(this->triggers_.hold[this->triggers_.selected_gate]);
    this->trigger_snr_number_->publish_state(this->triggers_.snr[this->triggers_.selected_gate]);
#endif
  };

#endif

#ifdef USE_BUTTON
  void set_read_all_button(button::Button *button) { this->read_all_button_ = button; };
  void set_write_all_button(button::Button *button) { this->write_all_button_ = button; };
  void set_calibration_button(button::Button *button) { this->calibration_button_ = button; };
  void set_factory_reset_button(button::Button *button) { this->factory_reset_button_ = button; };
  void set_toggle_minimal_output_button(button::Button *button) { this->toggle_minimal_output_button_ = button; };
    // void set_enable_config_button(button::Button* button) {
    // this->enable_config_button_ = button; }; void
    // set_disable_config_button(button::Button* button) {
    // this->disable_config_button_ = button; };
#endif

#ifdef USE_SELECT
  void set_response_speed_select(select::Select *selector) { this->response_speed_select_ = selector; };
#endif
 private:
  std::vector<LD2410SListener *> listeners_{};
  // Config current_config;
  bool cmd_active_{false};

  TriggersT triggers_;

  uint32_t max_dist_{0};     //
  uint32_t min_dist_{0};     //
  uint32_t delay_{0};        //
  uint32_t status_freq_{0};  //
  uint32_t dist_freq_{0};    //
  uint32_t resp_speed_{0};   //
  bool minimal_output_{true};
  uint32_t energy_values_[16];
  uint32_t energy_values_count_ = 0;
  std::string energy_values_str_ = "";

  uint8_t active_ = 0;
  uint8_t last_ = 0;
  CmdT commands_[CMD_EXEC_BUFFER_SIZE];

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
  // button::Button* enable_config_button{ nullptr };
  // button::Button* disable_config_button{ nullptr };
#endif

#ifdef USE_SELECT
  select::Select *response_speed_select_{nullptr};
#endif

  void receive_(uint8_t *buffer, size_t buffer_size, size_t &pos);
  PackageType get_frame_type_(uint8_t *buffer, size_t pos);
  size_t get_frame_start_(uint8_t *buffer, size_t end_pos, PackageType type);
  size_t get_data_size_(uint8_t *buffer, size_t end_pos, PackageType type, size_t start_pos);
  // size_t get_data_start_pos_(PackageType type, size_t start_pos);

  // bool process_frame_(PackageType type, uint8_t *buffer, size_t data_size);
  void process_short_data_frame_(uint8_t *data);
  void process_data_frame_(uint8_t *data);
  void process_cmd_frame_(uint8_t *buffer, size_t len);

  CmdAckT parse_cms_frame_(uint8_t *buffer, size_t length);

  // void process_data_distance_(uint8_t *data);
  // void process_data_progress_(uint8_t *data);
  // void process_data_energy_levels_(uint8_t *data);

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
  int two_byte_to_int_(uint8_t byte1, uint8_t byte2);
  uint32_t four_byte_to_int_(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4);
  uint16_t swap_endian_(uint16_t value);
  uint32_t swap_endian_(uint32_t value);
};

}  // namespace ld2410s
}  // namespace esphome
