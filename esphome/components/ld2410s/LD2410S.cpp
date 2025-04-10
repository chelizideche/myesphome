#include "esphome/core/log.h"
#include "LD2410S.h"

namespace esphome {
namespace ld2410s {

void LD2410S::setup() {
  this->minimal_output_ = true;
  this->send_cmd_frame_(CONFIG_MODE_START_CMD);
  this->send_cmd_frame_(OUTPUT_MODE_SWITCH_CMD);
  this->send_cmd_frame_(FW_READ_CMD);
  this->send_cmd_frame_(PARAMS_READ_CMD);
  this->send_cmd_frame_(GATE_TRIGGER_THRESHOLD_READ_CMD);
  this->send_cmd_frame_(GATE_HOLD_THRESHOLD_READ_CMD);
  this->send_cmd_frame_(GATE_SNR_READ_CMD);
  this->send_cmd_frame_(CONFIG_MODE_END_CMD);
}
void LD2410S::loop() {
  if (!this->cmd_active_) {
    if (available()) {
      static uint8_t buffer[128];
      static size_t pos = 0;
      bool reply = false;
      this->receive_(buffer, sizeof(buffer), pos, reply);
    } else {
      this->loop_exec_();
    }
  }
}

void LD2410S::loop_exec_() {
  CmdT *cmd = &commands_[this->active_];
  uint32_t now = millis();

  if (cmd->state == CmdState::SCHEDULED) {
    // ESP_LOGD(TAG, "loop_exec Send new command active:%d, last:%d", this->active, this->last_);
    this->send_command_(cmd->cmd_frame);
    cmd->state = CmdState::SENT;
    cmd->time_started = now;

  } else if (cmd->state == CmdState::SENT) {
    if (now >= cmd->time_started + CMD_EXEC_TIMEOUT) {
      if (cmd->retry < CMD_EXEC_REPEAT) {
        ESP_LOGD(TAG, "loop_exec Send Retry active:%d, last:%d", this->active_, this->last_);
        cmd->retry++;
        cmd->time_started = now;
        this->send_command_(cmd->cmd_frame);
      } else {
        ESP_LOGD(TAG, "loop_exec Send to Give up active:%d, last:%d", this->active_, this->last_);
        cmd->state = CmdState::EMPTY;
        this->cmd_buffer_finished_();
      }
    }
  } else if (cmd->state == CmdState::EMPTY & this->active_ == this->last_ & this->active_ != 0) {
    this->active_ = 0;
    this->last_ = 0;
    // ESP_LOGD(TAG, "loop_exec Sending Done");
  }
}

void LD2410S::cmd_add_(CmdFrameT *cmd_frame) {
  // ESP_LOGD(TAG, "cmd_add command:%0x, lengt:%d", cmd_frame->command, cmd_frame->length);
  CmdT cmd;
  cmd.state = CmdState::SCHEDULED;
  cmd.cmd_frame = cmd_frame;
  cmd.time_started = 0;
  cmd.retry = 0;

  this->cmd_buffer_insert_(&cmd);
}

void LD2410S::cmd_buffer_insert_(CmdT *cmd) {
  if (!cmd) {
    ESP_LOGD(TAG, "cmd_buffer_insert cmd is empty !!! active:%d, last:%d", this->active_, this->last_);
    return;
  }

  if (this->commands_[this->last_].state != CmdState::EMPTY) {
    uint8_t next = this->last_;
    this->cmd_buffer_inc_(next);
    if (this->commands_[next].state != CmdState::EMPTY) {
      ESP_LOGD(TAG, "cmd_buffer_insert Buffer FULL !!! active:%d, last:%d", this->active_, this->last_);
      return;
    }
    this->last_ = next;
  }

  this->commands_[this->last_] = *cmd;  // Shallow copy of state, time_started, retry

  if (cmd->cmd_frame) {
    this->commands_[this->last_].cmd_frame = new CmdFrameT(*cmd->cmd_frame);  // Deep copy
  } else {
    this->commands_[this->last_].cmd_frame = nullptr;
  }

  // ESP_LOGD(TAG, "cmd_buffer_insert  command:%0x, lengt:%d, active:%d, last:%d", cmd->cmd_frame->command,
}
void LD2410S::cmd_buffer_finished_() {
  this->commands_[this->active_].state = CmdState::EMPTY;

  if (this->commands_[this->active_ + 1].state != CmdState::EMPTY) {
    this->cmd_buffer_inc_(this->active_);
  }

  // ESP_LOGD(TAG, "cmd_buffer_finished active:%d, last:%d", this->active_, this->last_);
}

void LD2410S::cmd_buffer_inc_(uint8_t &index) {
  index++;
  if (index >= CMD_EXEC_BUFFER_SIZE) {
    index = 0;
  }
}

float LD2410S::get_setup_priority() const { return setup_priority::HARDWARE; }

void LD2410S::read_all() {
  this->status_set_warning("read_all");

  this->send_cmd_frame_(CONFIG_MODE_START_CMD);
  this->send_cmd_frame_(FW_READ_CMD);
  this->send_cmd_frame_(PARAMS_READ_CMD);
  this->send_cmd_frame_(GATE_TRIGGER_THRESHOLD_READ_CMD);
  this->send_cmd_frame_(GATE_HOLD_THRESHOLD_READ_CMD);
  this->send_cmd_frame_(GATE_SNR_READ_CMD);
  this->send_cmd_frame_(CONFIG_MODE_END_CMD);

  this->status_clear_warning();
}

void LD2410S::apply_config() {
  this->status_set_warning("apply_config");
  this->send_cmd_frame_(CONFIG_MODE_START_CMD);

  this->send_cmd_frame_(OUTPUT_MODE_SWITCH_CMD);
  this->send_cmd_frame_(PARAMS_WRITE_CMD);
  this->send_cmd_frame_(GATE_TRIGGER_THRESHOLD_WRITE_CMD);
  this->send_cmd_frame_(GATE_HOLD_THRESHOLD_WRITE_CMD);
  this->send_cmd_frame_(GATE_SNR_WRITE_CMD);

  this->send_cmd_frame_(PARAMS_READ_CMD);
  this->send_cmd_frame_(GATE_TRIGGER_THRESHOLD_READ_CMD);
  this->send_cmd_frame_(GATE_HOLD_THRESHOLD_READ_CMD);
  this->send_cmd_frame_(GATE_SNR_READ_CMD);

  this->send_cmd_frame_(CONFIG_MODE_END_CMD);
  this->status_clear_warning();
}

void LD2410S::calibration() { this->send_cmd_("start_calibration\0", CALIBRATION_CMD); }

void LD2410S::factory_reset() {
  this->minimal_output_ = true;

  this->max_dist_ = 16;
  this->min_dist_ = 0;
  this->delay_ = 10;
  this->status_freq_ = 80;
  this->dist_freq_ = 80;
  this->resp_speed_ = 5;

  for (int i = 0; i < 16; i++) {
    this->triggers_.trigger[i] = GATE_TRIGGER_THRESHOLD_WRITE_DATA[i];
    this->triggers_.hold[i] = GATE_HOLD_THRESHOLD_WRITE_DATA[i];
    this->triggers_.snr[i] = GATE_SNR_WRITE_DATA[i];
  }

  this->status_set_warning("factory_reset");
  this->send_cmd_frame_(CONFIG_MODE_START_CMD);

  this->send_cmd_frame_(OUTPUT_MODE_SWITCH_CMD);

  this->send_cmd_frame_(PARAMS_WRITE_CMD);
  this->send_cmd_frame_(GATE_TRIGGER_THRESHOLD_WRITE_CMD);
  this->send_cmd_frame_(GATE_HOLD_THRESHOLD_WRITE_CMD);
  this->send_cmd_frame_(GATE_SNR_WRITE_CMD);

  this->send_cmd_frame_(PARAMS_READ_CMD);
  this->send_cmd_frame_(GATE_TRIGGER_THRESHOLD_READ_CMD);
  this->send_cmd_frame_(GATE_HOLD_THRESHOLD_READ_CMD);
  this->send_cmd_frame_(GATE_SNR_READ_CMD);

  this->send_cmd_frame_(CONFIG_MODE_END_CMD);
  this->status_clear_warning();
}

void LD2410S::toggle_minimal() {
  this->minimal_output_ = !this->minimal_output_;
  this->send_cmd_("toggle_minimal_output\0", OUTPUT_MODE_SWITCH_CMD);
}

void LD2410S::set_delay(float delay) {
  this->delay_ = delay;
  this->send_cmd_("set_delay\0", PARAMS_WRITE_CMD, CFG_NO_DELAY_VALUE);
}
void LD2410S::set_distance_reporting_freq(float distance_reporting_freq) {
  this->dist_freq_ = distance_reporting_freq * 10;
  this->send_cmd_("set_distance_reporting_freq\0", PARAMS_WRITE_CMD, CFG_DISTANCE_FREQ_VALUE);
}
void LD2410S::set_max_distance(float max_distance) {
  this->max_dist_ = static_cast<float>(max_distance) / 0.7;
  this->send_cmd_("set_max_distance\0", PARAMS_WRITE_CMD, CFG_MAX_DETECTION_VALUE);
}
void LD2410S::set_min_distance(float min_distance) {
  this->min_dist_ = static_cast<float>(min_distance) / 0.7;
  this->send_cmd_("set_min_distance\0", PARAMS_WRITE_CMD, CFG_MIN_DETECTION_VALUE);
}
void LD2410S::set_status_reporting_freq(float status_reporting_freq) {
  this->status_freq_ = status_reporting_freq * 10;
  this->send_cmd_("set_status_reporting_freq\0", PARAMS_WRITE_CMD, CFG_STATUS_FREQ_VALUE);
}
void LD2410S::set_trigger_selected_gate(float trigger_selected_gate) {
  this->triggers_.selected_gate = trigger_selected_gate;
#ifdef USE_NUMBER
  this->trigger_selected_gate_number_->publish_state(this->triggers_.selected_gate);
  this->trigger_threshold_number_->publish_state(this->triggers_.trigger[this->triggers_.selected_gate]);
  this->trigger_hold_number_->publish_state(this->triggers_.hold[this->triggers_.selected_gate]);
  this->trigger_snr_number_->publish_state(this->triggers_.snr[this->triggers_.selected_gate]);
#endif
}
void LD2410S::set_trigger_threshold(float trigger_threshold) {
  this->triggers_.trigger[this->triggers_.selected_gate] = trigger_threshold;
  this->send_cmd_("set_trigger_threshold\0", GATE_TRIGGER_THRESHOLD_WRITE_CMD, this->triggers_.selected_gate,
                  GATE_TRIGGER_THRESHOLD_READ_CMD);
}
void LD2410S::set_trigger_hold(float trigger_hold) {
  this->triggers_.hold[this->triggers_.selected_gate] = trigger_hold;
  this->send_cmd_("set_trigger_hold\0", GATE_HOLD_THRESHOLD_WRITE_CMD, this->triggers_.selected_gate,
                  GATE_HOLD_THRESHOLD_READ_CMD);
}
void LD2410S::set_trigger_snr(float trigger_snr) {
  this->triggers_.snr[this->triggers_.selected_gate] = trigger_snr;
  this->send_cmd_("set_trigger_snr\0", GATE_SNR_WRITE_CMD, this->triggers_.selected_gate, GATE_SNR_READ_CMD);
}
void LD2410S::set_response_speed_select(const std::string &response_speed_select) {
  this->resp_speed_ = response_speed_select == RESPONSE_SPEED_NORMAL ? 5 : 10;
  this->send_cmd_("set_response_speed_select\0", PARAMS_WRITE_CMD, CFG_RESPONSE_SPEED_VALUE);
}

void LD2410S::send_cmd_(const char *msg, uint16_t command, uint16_t sub_command, uint16_t command2,
                        uint16_t sub_command2) {
  this->status_set_warning(msg);

  this->send_cmd_frame_(CONFIG_MODE_START_CMD);
  this->send_cmd_frame_(command, sub_command);
  if (command2 != 0xffff && sub_command2 != 16) {
    this->send_cmd_frame_(command2, sub_command2);
  }
  this->send_cmd_frame_(CONFIG_MODE_END_CMD);

  this->status_clear_warning();
}

void LD2410S::send_cmd_frame_(uint16_t command, uint16_t sub_command) {
  ESP_LOGD(TAG, "send_cmd_frame %x : %x", command, sub_command);

  CmdFrameT cmd_frame = {.header = CMD_FRAME_HEADER, .command = command, .data_length = 0, .footer = CMD_FRAME_FOOTER};

  switch (command) {
    case OUTPUT_MODE_SWITCH_CMD: {
      if (this->minimal_output_) {
        this->cmd_frame_append_data_(&cmd_frame, &OUTPUT_MODE_VALUE_MIN[0], 4);
      } else {
        this->cmd_frame_append_data_(&cmd_frame, &OUTPUT_MODE_VALUE_STD[0], 4);
      }
    } break;

    case CONFIG_MODE_START_CMD:
      this->cmd_frame_append_data_(&cmd_frame, &CONFIG_MODE_START_VALUE[0], 2);
      break;

    case CONFIG_MODE_END_CMD:
      break;

    case PARAMS_READ_CMD:

      switch (sub_command) {
        case CFG_MAX_DETECTION_VALUE:
          this->cmd_frame_append_data_(&cmd_frame, &CFG_MAX_DETECTION_VALUE, 1);
          break;

        case CFG_MIN_DETECTION_VALUE:
          this->cmd_frame_append_data_(&cmd_frame, &CFG_MIN_DETECTION_VALUE, 1);
          break;

        case CFG_NO_DELAY_VALUE:
          this->cmd_frame_append_data_(&cmd_frame, &CFG_NO_DELAY_VALUE, 1);
          break;

        case CFG_STATUS_FREQ_VALUE:
          this->cmd_frame_append_data_(&cmd_frame, &CFG_STATUS_FREQ_VALUE, 1);
          break;

        case CFG_DISTANCE_FREQ_VALUE:
          this->cmd_frame_append_data_(&cmd_frame, &CFG_DISTANCE_FREQ_VALUE, 1);
          break;

        case CFG_RESPONSE_SPEED_VALUE:
          this->cmd_frame_append_data_(&cmd_frame, &CFG_RESPONSE_SPEED_VALUE, 1);
          break;

        default:
          this->cmd_frame_append_data_(&cmd_frame, &CFG_MAX_DETECTION_VALUE, 1);
          this->cmd_frame_append_data_(&cmd_frame, &CFG_MIN_DETECTION_VALUE, 1);
          this->cmd_frame_append_data_(&cmd_frame, &CFG_NO_DELAY_VALUE, 1);
          this->cmd_frame_append_data_(&cmd_frame, &CFG_STATUS_FREQ_VALUE, 1);
          this->cmd_frame_append_data_(&cmd_frame, &CFG_DISTANCE_FREQ_VALUE, 1);
          this->cmd_frame_append_data_(&cmd_frame, &CFG_RESPONSE_SPEED_VALUE, 1);
          break;
      }

      break;

    case FW_READ_CMD:
      break;

    case PARAMS_WRITE_CMD:
      if (this->resp_speed_ == 0) {
        ESP_LOGD(TAG, "PARAMS_WRITE_CMD Error, bad new_config");
        return;
      } else {
        switch (sub_command) {
          case CFG_MAX_DETECTION_VALUE:
            this->cmd_frame_append_data_(&cmd_frame, &CFG_MAX_DETECTION_VALUE, 1);
            this->cmd_frame_append_data_(&cmd_frame, &this->max_dist_, 1);
            break;

          case CFG_MIN_DETECTION_VALUE:
            this->cmd_frame_append_data_(&cmd_frame, &CFG_MIN_DETECTION_VALUE, 1);
            this->cmd_frame_append_data_(&cmd_frame, &this->min_dist_, 1);
            break;

          case CFG_NO_DELAY_VALUE:
            this->cmd_frame_append_data_(&cmd_frame, &CFG_NO_DELAY_VALUE, 1);
            this->cmd_frame_append_data_(&cmd_frame, &this->delay_, 1);
            break;

          case CFG_STATUS_FREQ_VALUE:
            this->cmd_frame_append_data_(&cmd_frame, &CFG_STATUS_FREQ_VALUE, 1);
            this->cmd_frame_append_data_(&cmd_frame, &this->status_freq_, 1);
            break;

          case CFG_DISTANCE_FREQ_VALUE:
            this->cmd_frame_append_data_(&cmd_frame, &CFG_DISTANCE_FREQ_VALUE, 1);
            this->cmd_frame_append_data_(&cmd_frame, &this->dist_freq_, 1);
            break;

          case CFG_RESPONSE_SPEED_VALUE:
            this->cmd_frame_append_data_(&cmd_frame, &CFG_RESPONSE_SPEED_VALUE, 1);
            this->cmd_frame_append_data_(&cmd_frame, &this->resp_speed_, 1);
            break;
          default:

            this->cmd_frame_append_data_(&cmd_frame, &CFG_MAX_DETECTION_VALUE, 1);
            this->cmd_frame_append_data_(&cmd_frame, &this->max_dist_, 1);

            this->cmd_frame_append_data_(&cmd_frame, &CFG_MIN_DETECTION_VALUE, 1);
            this->cmd_frame_append_data_(&cmd_frame, &this->min_dist_, 1);

            this->cmd_frame_append_data_(&cmd_frame, &CFG_NO_DELAY_VALUE, 1);
            this->cmd_frame_append_data_(&cmd_frame, &this->delay_, 1);

            this->cmd_frame_append_data_(&cmd_frame, &CFG_STATUS_FREQ_VALUE, 1);
            this->cmd_frame_append_data_(&cmd_frame, &this->status_freq_, 1);

            this->cmd_frame_append_data_(&cmd_frame, &CFG_DISTANCE_FREQ_VALUE, 1);
            this->cmd_frame_append_data_(&cmd_frame, &this->dist_freq_, 1);

            this->cmd_frame_append_data_(&cmd_frame, &CFG_RESPONSE_SPEED_VALUE, 1);
            this->cmd_frame_append_data_(&cmd_frame, &this->resp_speed_, 1);
            break;
        }
        break;
      }

    case CALIBRATION_CMD:
      this->cmd_frame_append_data_(&cmd_frame, &CALIBRATION_TRIGGER_VALUE, 1);
      this->cmd_frame_append_data_(&cmd_frame, &CALIBRATION_RETENTION_VALUE, 1);
      this->cmd_frame_append_data_(&cmd_frame, &CALIBRATION_TIME_VALUE, 1);
      break;

    case GATE_TRIGGER_THRESHOLD_READ_CMD:
    case GATE_HOLD_THRESHOLD_READ_CMD:
    case GATE_SNR_READ_CMD:
      if (sub_command < 16) {
        this->cmd_frame_append_data_(&cmd_frame, &sub_command, 1);
      } else {
        for (uint16_t i = 0; i < 16; i++) {
          this->cmd_frame_append_data_(&cmd_frame, &i, 1);
        }
      }
      break;

    case GATE_TRIGGER_THRESHOLD_WRITE_CMD:
      if (sub_command < 16) {
        this->cmd_frame_append_data_(&cmd_frame, &sub_command, 1);
        this->cmd_frame_append_data_(&cmd_frame, &this->triggers_.trigger[sub_command], 1);
      } else {
        for (uint16_t i = 0; i < 16; i++) {
          this->cmd_frame_append_data_(&cmd_frame, &i, 1);
          this->cmd_frame_append_data_(&cmd_frame, &this->triggers_.trigger[i], 1);
        }
      }
      break;

    case GATE_HOLD_THRESHOLD_WRITE_CMD:
      if (sub_command < 16) {
        this->cmd_frame_append_data_(&cmd_frame, &sub_command, 1);
        this->cmd_frame_append_data_(&cmd_frame, &this->triggers_.hold[sub_command], 1);
      } else {
        for (uint16_t i = 0; i < 16; i++) {
          this->cmd_frame_append_data_(&cmd_frame, &i, 1);
          this->cmd_frame_append_data_(&cmd_frame, &this->triggers_.hold[i], 1);
        }
      }
      break;

    case GATE_SNR_WRITE_CMD:
      if (sub_command < 16) {
        this->cmd_frame_append_data_(&cmd_frame, &sub_command, 1);
        this->cmd_frame_append_data_(&cmd_frame, &this->triggers_.snr[sub_command], 1);
      } else {
        for (uint16_t i = 0; i < 16; i++) {
          this->cmd_frame_append_data_(&cmd_frame, &i, 1);
          this->cmd_frame_append_data_(&cmd_frame, &this->triggers_.snr[i], 1);
        }
      }
      break;

    default:

      break;
  }

  // ESP_LOGD(TAG, "send_cmd_frame frame_data_adr:%04x", &cmd_frame.data[0]);
  // this->hex_diag_("send_cmd_frame:", &cmd_frame.data[0], cmd_frame.data_length);

  this->cmd_add_(&cmd_frame);
  // this->send_command_(cmd_frame);
}

void LD2410S::cmd_frame_append_data_(CmdFrameT *cmd_frame, const uint8_t *append_data, size_t append_data_size) {
  memcpy(&cmd_frame->data[0] + cmd_frame->data_length * sizeof(cmd_frame->data[0]), append_data,
         append_data_size * sizeof(*append_data));

  cmd_frame->data_length = cmd_frame->data_length + append_data_size * sizeof(*append_data);
}
void LD2410S::cmd_frame_append_data_(CmdFrameT *cmd_frame, const uint16_t *append_data, size_t append_data_size) {
  memcpy(&cmd_frame->data[0] + cmd_frame->data_length * sizeof(cmd_frame->data[0]), append_data,
         append_data_size * sizeof(*append_data));

  cmd_frame->data_length = cmd_frame->data_length + append_data_size * sizeof(*append_data);
}
void LD2410S::cmd_frame_append_data_(CmdFrameT *cmd_frame, const uint32_t *append_data, size_t append_data_size) {
  memcpy(&cmd_frame->data[0] + cmd_frame->data_length * sizeof(cmd_frame->data[0]), append_data,
         append_data_size * sizeof(*append_data));

  cmd_frame->data_length = cmd_frame->data_length + append_data_size * sizeof(*append_data);
}

void LD2410S::send_command_(CmdFrameT *frame) {
  // ESP_LOGD(TAG, "Sending command Cmd:%04X, data_length:%d", frame->command, frame->data_length);
  // this->hex_diag_("Sending command:", &frame->data[0], frame->data_length);

  this->cmd_active_ = true;
  uint8_t cmd_buffer[128];

  frame->length = 0;
  uint16_t frame_data_bytes = frame->data_length + 2;
  // HEADER
  memcpy(&cmd_buffer[frame->length], &frame->header, sizeof(frame->header));
  frame->length += sizeof(frame->header);
  // SIZE
  memcpy(&cmd_buffer[frame->length], &frame_data_bytes, sizeof(frame->data_length));
  frame->length += sizeof(frame->data_length);
  // COMMAND
  memcpy(&cmd_buffer[frame->length], &frame->command, sizeof(frame->command));
  frame->length += sizeof(frame->command);
  // DATA
  for (uint16_t index = 0; index < frame->data_length; index++) {
    memcpy(&cmd_buffer[frame->length], &frame->data[index], sizeof(frame->data[index]));
    frame->length += sizeof(frame->data[index]);
  }
  // FOOTER
  memcpy(cmd_buffer + frame->length, &frame->footer, sizeof(frame->footer));
  frame->length += sizeof(frame->footer);

  this->hex_diag_(">", cmd_buffer, frame->length);

  // WRITE
  for (uint16_t index = 0; index < frame->length; index++) {
    this->write_byte(cmd_buffer[index]);
  }

  this->flush();

  this->cmd_active_ = false;
}

void LD2410S::receive_(uint8_t *buffer, size_t buffer_size, size_t &end_pos, bool &reply) {
  while (available()) {
    buffer[end_pos] = read();

    PackageType type = this->get_frame_type_(buffer, end_pos);
    if (type != PackageType::UNKNOWN) {
      size_t start_pos = get_frame_start_(buffer, end_pos, type);

      if (start_pos != end_pos) {
        size_t data_size = get_data_size_(buffer, end_pos, type, start_pos);

        if (data_size != 0) {
          this->hex_diag_("<", &buffer[0], end_pos + 1 - start_pos);

          switch (type) {
            case PackageType::SHORT_DATA_FRAME:
              this->process_short_data_frame_(&buffer[start_pos + 1]);
              break;
            case PackageType::STD_DATA_FRAME:
              this->process_data_frame_(&buffer[start_pos + 6]);
              break;
            case PackageType::CMD_FRAME:
              reply = this->process_cmd_frame_(buffer, end_pos + 1);
              this->cmd_buffer_finished_();
              break;
            case PackageType::BAD_SIZE:
              this->hex_diag_("Received BAD sized package <", &buffer[0], end_pos + 1 - start_pos);
              break;
            default:
              this->hex_diag_("Received Unknown Package <", &buffer[0], end_pos + 1 - start_pos);
              break;
          }
          //         ESP_LOGD(TAG, "after process package: reply:%s", reply);
        }
      }

      end_pos = 0;
    } else {
      end_pos++;
      if (end_pos >= buffer_size - 1) {
        end_pos = 0;
      }
    }
  }
}

PackageType LD2410S::get_frame_type_(uint8_t *buffer, size_t end_pos) {
  if (end_pos < 4) {
    return PackageType::UNKNOWN;
  }
  if (buffer[end_pos] == SHORT_DATA_FRAME_FOOTER) {
    return PackageType::SHORT_DATA_FRAME;
  }
  if (end_pos < 10) {
    return PackageType::UNKNOWN;
  }
  if (memcmp(&buffer[end_pos - 3], &STD_DATA_FRAME_FOOTER, sizeof(STD_DATA_FRAME_FOOTER)) == 0) {
    return PackageType::STD_DATA_FRAME;
  }
  if (memcmp(&buffer[end_pos - 3], &CMD_FRAME_FOOTER, sizeof(CMD_FRAME_FOOTER)) == 0) {
    return PackageType::CMD_FRAME;
  }
  return PackageType::UNKNOWN;
}
size_t LD2410S::get_frame_start_(uint8_t *buffer, size_t end_pos, PackageType type) {
  size_t min_length = 0;
  uint32_t header_frame = 0;
  int header_frame_len = 0;

  switch (type) {
    case PackageType::SHORT_DATA_FRAME:
      min_length = 4;
      header_frame = SHORT_DATA_FRAME_HEADER;
      header_frame_len = sizeof(SHORT_DATA_FRAME_HEADER);
      break;

    case PackageType::STD_DATA_FRAME:
      min_length = 12;
      header_frame = STD_DATA_FRAME_HEADER;
      header_frame_len = sizeof(STD_DATA_FRAME_HEADER);
      break;

    case PackageType::CMD_FRAME:
      min_length = 12;
      header_frame = CMD_FRAME_HEADER;
      header_frame_len = sizeof(CMD_FRAME_HEADER);
      break;

    default:
      return end_pos;
      break;
  }

  if (end_pos + 1 < min_length) {
    return end_pos;
  }

  for (int i = end_pos - min_length; i >= 0; i--) {
    if (header_frame == read_int_(buffer, i, header_frame_len)) {
      return i;
    }
  }

  return end_pos;
}
size_t LD2410S::get_data_size_(uint8_t *buffer, size_t end_pos, PackageType type, size_t start_pos) {
  size_t data_size = 0;
  size_t expected_full_frame_size = 0;

  switch (type) {
    case PackageType::SHORT_DATA_FRAME:
      data_size = 3;
      expected_full_frame_size = 1 + data_size + 1;
      break;

    case PackageType::STD_DATA_FRAME:
    case PackageType::CMD_FRAME:
      data_size = read_int_(buffer, start_pos + 4, 2);
      expected_full_frame_size = 4 + 2 + data_size + 4;
      break;

    default:
      break;
  }

  if (data_size == 0) {
    return 0;
  }

  // ESP_LOGD(TAG, "< start_pos:%d,  end_pos:%d, data_size:%d, expected_full_frame_size:%d, actual_frame_size:%d",
  //     start_pos, end_pos, data_size,expected_full_frame_size, end_pos - start_pos + 1);

  if (expected_full_frame_size != end_pos - start_pos + 1) {
    return 0;
  }

  return data_size;
}

void LD2410S::process_short_data_frame_(uint8_t *data) {
  //            ESP_LOGD(TAG, "process_short_data_package");

  const bool presence_state = data[0] > 1;
  int distance = this->two_byte_to_int_(data[1], data[2]);
  if (!presence_state)
    distance = 0;
  // ESP_LOGD(TAG, "Presence: %x , Distance: %i", presence_state, distance);
  for (auto &listener : this->listeners_) {
    listener->on_presence(presence_state);
    listener->on_distance(distance);
  }
}
void LD2410S::process_data_frame_(uint8_t *data) {
  switch (data[0]) {
    case 0x01:  // standard data
    {
      const bool presence_state = data[1] > 1;
      int distance = this->two_byte_to_int_(data[2], data[3]);
      if (!presence_state)
        distance = 0;
      // ESP_LOGD(TAG, "Presence: %x , Distance: %i", presence_state, distance);
      for (auto &listener : this->listeners_) {
        listener->on_presence(presence_state);
        listener->on_distance(distance);
      }
      // this->hex_diag_("energy < ", &data[6], 64);
      this->process_data_energy_values_read_(&data[6]);
      break;
    }

    case 0x03:  // calibration progress
    {
      int progress = this->two_byte_to_int_(data[1], data[2]);
      for (auto &listener : this->listeners_) {
        if (progress == 100) {
          listener->on_threshold_progress(0);
          listener->on_threshold_update(false);
          this->read_all();
        } else {
          listener->on_threshold_progress(progress);
          listener->on_threshold_update(true);
        }
      }
      break;
    }

    default:
      break;
  }
}
bool LD2410S::process_cmd_frame_(uint8_t *buffer, size_t len) {
  CmdAckT ack = this->parse_ack_(buffer, len);
  int command_word = ack.command;
  bool result = ack.result;
  if (!result) {
    ESP_LOGW(TAG, "Command %x failed", command_word);
    return false;
  }

  uint8_t *data = ack.data;

  switch (command_word) {
    case PARAMS_READ_REPLY:
      this->process_config_read_ack_(data);
      break;

    case FW_READ_REPLY:
      this->process_ack_fw_read_(data);
      break;

    case GATE_TRIGGER_THRESHOLD_READ_REPLY:
      this->process_ack_trigger_threshold_read_(data);
      break;

    case GATE_HOLD_THRESHOLD_READ_REPLY:
      this->process_ack_trigger_hold_read_(data);
      break;

    case GATE_SNR_READ_REPLY:
      this->process_ack_trigger_snr_read_(data);
      break;

    case CONFIG_MODE_START_REPLY:
      ESP_LOGI(TAG, "Config mode enabled");
      break;

    case CONFIG_MODE_END_REPLY:
      ESP_LOGI(TAG, "Config mode disabled");
      break;

    case PARAMS_WRITE_REPLY:
      ESP_LOGI(TAG, "Config written");
      break;

    case GATE_TRIGGER_THRESHOLD_WRITE_REPLY:
      ESP_LOGI(TAG, "Trigger Thrashold written");
      break;

    case GATE_HOLD_THRESHOLD_WRITE_REPLY:
      ESP_LOGI(TAG, "Trigger Hold written");
      break;

    case GATE_SNR_WRITE_REPLY:
      ESP_LOGI(TAG, "Trigger SNR written");
      break;

    case OUTPUT_MODE_SWITCH_REPLY:
      ESP_LOGI(TAG, "Minimal Output Mode switched");
      break;

    default:
      ESP_LOGW(TAG, "< Unknown: %4x", command_word);
      break;
  }

  return true;
}

CmdAckT LD2410S::parse_ack_(uint8_t *buffer, size_t length) {
  CmdAckT result;
  size_t start = -1;
  for (size_t i = 0; i < length; i++) {
    if (memcmp(&buffer[i], &CMD_FRAME_HEADER, sizeof(CMD_FRAME_HEADER)) == 0) {
      start = i;
      break;
    }
  }
  if (start == -1) {
    ESP_LOGE(TAG, "Can't find cmd header");
    result.result = false;
    return result;
  }
  int data_length = this->two_byte_to_int_(buffer[start + 4], buffer[start + 5]);
  result.length = data_length;
  int command_word = this->two_byte_to_int_(buffer[start + 6], buffer[start + 7]);
  result.command = command_word;
  bool ack = buffer[start + 8] == 0x00 && buffer[start + 9] == 0x00;
  result.result = ack;
  for (size_t idx = 0; idx < result.length; idx++) {
    memcpy(&result.data[idx], &buffer[idx + 10], sizeof(buffer[idx + 10]));
  }
  return result;
}

void LD2410S::process_config_read_ack_(uint8_t *data) {
  this->max_dist_ = this->read_int_(data, 0, 4);
  this->min_dist_ = this->read_int_(data, 4, 4);
  this->delay_ = this->read_int_(data, 8, 4);
  this->status_freq_ = this->read_int_(data, 12, 4);
  this->dist_freq_ = this->read_int_(data, 16, 4);
  this->resp_speed_ = this->read_int_(data, 20, 4);

#ifdef USE_NUMBER
  this->max_distance_number_->publish_state(static_cast<float>(this->max_dist_) * 0.7);
  this->min_distance_number_->publish_state(static_cast<float>(this->min_dist_) * 0.7);
  this->no_delay_number_->publish_state(this->delay_);
  this->status_reporting_freq_number_->publish_state(static_cast<float>(this->status_freq_) / 10);
  this->distance_reporting_freq_number_->publish_state(static_cast<float>(this->dist_freq_) / 10);
#endif

#ifdef USE_SELECT
  this->response_speed_select_->publish_state(this->resp_speed_ == 5 ? RESPONSE_SPEED_NORMAL : RESPONSE_SPEED_FAST);
#endif

  ESP_LOGI(TAG, "Config: max_dist=%d, min_dist=%d, delay=%d, status_resp_freq=%d, dist_resp_freq=%d, resp_speed=%d",
           this->max_dist_, this->min_dist_, this->delay_, this->status_freq_, this->dist_freq_, this->resp_speed_);
}

void LD2410S::process_ack_fw_read_(const uint8_t *data) {
  int major_v = static_cast<int>(data[0]);
  int minor_v = static_cast<int>(data[1]);
  int patch_v = static_cast<int>(data[2]);
  std::string version = "v" + std::to_string(major_v) + "." + std::to_string(minor_v) + "." + std::to_string(patch_v);

  for (auto &listener : this->listeners_) {
    listener->on_fw_version(version);
  }

  ESP_LOGD(TAG, "Firmware version: %s", version.c_str());
}

void LD2410S::process_ack_trigger_threshold_read_(uint8_t *data) {
  this->four_byte_to_int_array_(data, this->triggers_.trigger, 16);
#ifdef USE_NUMBER
  this->trigger_threshold_number_->publish_state(this->triggers_.trigger[this->triggers_.selected_gate]);
#endif

  std::string vals = this->format_int_(this->triggers_.trigger, 16, 2);

  for (auto &listener : this->listeners_) {
    listener->on_trigger_threshold_ts(vals);
  }

  ESP_LOGD(TAG, "Gate Trigger Thresholds: %s", vals.c_str());
}

void LD2410S::process_ack_trigger_hold_read_(uint8_t *data) {
  this->four_byte_to_int_array_(data, this->triggers_.hold, 16);
#ifdef USE_NUMBER
  this->trigger_hold_number_->publish_state(this->triggers_.hold[this->triggers_.selected_gate]);
#endif

  std::string vals = this->format_int_(this->triggers_.hold, 16, 2);

  for (auto &listener : this->listeners_) {
    listener->on_trigger_hold_ts(vals);
  }

  ESP_LOGD(TAG, "Gate Trigger Holds: %s", vals.c_str());
}

void LD2410S::process_ack_trigger_snr_read_(uint8_t *data) {
  this->four_byte_to_int_array_(data, this->triggers_.snr, 16);
#ifdef USE_NUMBER
  this->trigger_snr_number_->publish_state(this->triggers_.snr[this->triggers_.selected_gate]);
#endif

  std::string vals = this->format_int_(this->triggers_.snr, 16, 2);

  for (auto &listener : this->listeners_) {
    listener->on_trigger_snr_ts(vals);
  }

  ESP_LOGD(TAG, "Gate Trigger SNR: %s", vals.c_str());
}

void LD2410S::process_data_energy_values_read_(uint8_t *data) {
  for (int i = 0; i < 16; i++) {
    uint32_t val = this->four_byte_to_int_(data[i * 4], data[i * 4 + 1], data[i * 4 + 2], data[i * 4 + 3]);
    this->energy_values_[i] =
        (this->energy_values_[i] * 16 * this->energy_values_count_ + val) / (this->energy_values_count_ + 1) / 16;
    this->energy_values_count_++;
  }

  std::string vals = this->format_int_(this->energy_values_, 16, 3);

  if (energy_values_str_ != vals) {
    for (auto &listener : this->listeners_) {
      listener->on_energy_values_ts(vals);
    }

    energy_values_str_ = vals;
  }

  ESP_LOGD(TAG, "Energy Values: %s", vals.c_str());
}

std::string LD2410S::format_int_(uint32_t *in, uint8_t len, uint8_t min_w) {
  if (len == 0)
    return "";

  std::ostringstream oss;
  for (uint8_t i = 0; i < len; ++i) {
    if (i > 0)
      oss << ",";
    oss << std::setw(min_w) << std::setfill('0') << in[i];
  }

  return oss.str();
}

void LD2410S::four_byte_to_int_array_(uint8_t *in, uint32_t *out, uint8_t out_len) {
  for (int i = 0; i < out_len; i++) {
    out[i] = this->four_byte_to_int_(in[i * 4], in[i * 4 + 1], in[i * 4 + 2], in[i * 4 + 3]);
  }
}

void LD2410S::hex_diag_(const char *msg, const uint8_t *data, size_t length) {
  char output[length * 3 + 1];

  for (size_t i = 0; i < length; i++) {
    if (i > 0) {  // Add a space before each byte except the first one
      sprintf(output + (i * 3 - 1), " ");
    }
    sprintf(output + (i * 3), "%02X", data[i]);
  }

  output[length * 3 - 1] = '\0';  // Null-terminate the string

  ESP_LOGD(TAG, "%s %s ", msg, output);
}

int LD2410S::read_int_(const uint8_t *buffer, size_t pos, size_t len) {
  unsigned int ret = 0;
  int shift = 0;
  for (size_t i = 0; i < len; i++) {
    ret |= static_cast<unsigned int>(buffer[pos + i]) << shift;
    shift += 8;
  }
  return ret;
};
int LD2410S::two_byte_to_int_(uint8_t byte1, uint8_t byte2) { return byte1 | (byte2 << 8); };
uint32_t LD2410S::four_byte_to_int_(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4) {
  return byte1 | (byte2 << 8) | (byte3 << 16) | (byte4 << 24);
};
uint16_t LD2410S::swap_endian_(uint16_t value) { return (value >> 8) | (value << 8); }
uint32_t LD2410S::swap_endian_(uint32_t value) {
  return ((value >> 24) & 0x000000FF) | ((value >> 8) & 0x0000FF00) | ((value << 8) & 0x00FF0000) |
         ((value << 24) & 0xFF000000);
}
}  // namespace ld2410s
}  // namespace esphome
