#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

// Honeywell particulate matter sensor component
// Implementation for compact serie sensors HPMA115C0-003 and HPMA115C0-004 only
// According to specification in document 32322550 Issue G
// Datasheet named 'sps-siot-particulate-hpm-series-datasheet-32322550-ciid-165855.pdf'

// Development structure based on ESPHome component ld2420

#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif  // USE_NUMBER

#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif

namespace esphome {
namespace hpma115c0 {

// Local types and constants

// Component error codes from core module
enum _Error {
  ERROR_SUCCESS = 0,               // No error
  ERROR_TIMEOUT,                   // Communication timeout
  ERROR_CRC,                       // CRC error detected
  ERROR_NEGATIVE_ACK,              // Negative ack returned by sensor
  ERROR_UNEXPECTED_REPLY,          // Unexpected data in protocol
  ERROR_DATA_LENGTH_TOO_LONG,      // Data length field too long
  ERROR_COMMAND_MISMATCH,          // Command in reply does not match request
  ERROR_INVALID_REPLY_LENGTH,      // Unexpected length in reply
  ERROR_UNKNOWN_COMMAND,           // Unknown commmand
  ERROR_COEFFICIENT_OUT_OF_RANGE,  // Adjustment coefficient out of range
  ERROR_NULL_PARAMETER,            // Null parameter in function
};
using Error_t = enum _Error;

// Adjustment coefficient for compensation,
using AdjustmentCoefficient_t = uint8_t;
const static AdjustmentCoefficient_t ADJUSTMENT_COEFFICIENT_MIN = 30;
const static AdjustmentCoefficient_t ADJUSTMENT_COEFFICIENT_DEFAULT = 100;
const static AdjustmentCoefficient_t ADJUSTMENT_COEFFICIENT_MAX = 200;

// For callbacks from sensor domain
class Hpma115C0Listener {
 public:
  virtual void on_new_values(float pm_1_0, float pm_2_5, float pm_4_0, float pm_10_0, float aqi_2_5, float aqi_10_0){};
};

// Main PollingComponent class
class Hpma115C0PollingComponent : public PollingComponent, public uart::UARTDevice {
 public:
  Hpma115C0PollingComponent() = default;

  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

#ifdef USE_NUMBER
  void set_adjustment_coefficient(number::Number *number) { this->adjustment_coefficient_number_ = number; };
#endif  // USE_NUMBER

#ifdef USE_BUTTON
  void set_factory_default_button(button::Button *button) { this->factory_default_button_ = button; };
#endif

  void update_adjustment_coefficient(float new_value) {
    this->set_customer_adjustment_coefficient_((AdjustmentCoefficient_t) new_value);
    adjustment_coefficient_needs_refresh_ = true;
  };

  void factory_reset_action() {
    this->reset_customer_adjustment_coefficient_();
    this->adjustment_coefficient_needs_refresh_ = true;
  }

  void register_listener(Hpma115C0Listener *listener) { this->listeners_.push_back(listener); }

  AdjustmentCoefficient_t get_adjustment_coefficient_min_value() { return ADJUSTMENT_COEFFICIENT_MIN; }
  AdjustmentCoefficient_t get_adjustment_coefficient_max_value() { return ADJUSTMENT_COEFFICIENT_MAX; }

 protected:
  // Protected internal class types and constants

  // Documented commands
  enum _Cmd {
    CMD_READ_PARTICLE_MEASURING_RESULTS = 0x04,       // Read values
    CMD_START_PARTICLE_MEASUREMENT = 0x01,            // Start measuring, fan is turned on
    CMD_STOP_PARTICLE_MEASUREMENT = 0x02,             // Stop measuring, fan is turned off
    CMD_SET_CUSTOMER_ADJUSTMENT_COEFFICIENT = 0x08,   // Change adjustment coefficient, values in [30; 200]
    CMD_READ_CUSTOMER_ADJUSTMENT_COEFFICIENT = 0x10,  // Read current adjustment coefficient
    CMD_STOP_AUTO_SEND = 0x20,                        // Stop autosend mode
    CMD_ENABLE_AUTO_SEND = 0x40,                      // Start autosend mode
  };
  using Cmd_t = uint8_t;

  // Frame types : request or reply (with data)
  // Serial communication frame type, common for both requests and replies
  enum _FrameType {
    FRAME_TYPE_CMD = 0x68,              // Command
    FRAME_TYPE_REPLY_WITH_DATA = 0x40,  // Reply, positive ack with data
  };
  using FrameType_t = uint8_t;

  // Max value returned by sensor in µg/m3
  const static int16_t MAX_PM_CONCENTRATION = 1000;

  // Max communication reply time in ms
  const static uint16_t MAX_ALLOWED_REPLY_DELAY = 50;

  // Max number of activation attempts
  const static uint16_t MAX_ACTIVATION_ATTEMPTS = 10;

  // Reply codes with no data : Neg(ative) or Pos(itive) ACK
  const static uint16_t RESPONSE_POS_ACK = 0xA5A5;
  const static uint16_t RESPONSE_NEG_ACK = 0x9696;

  // Max data length for a request / reply frame
  const static uint8_t MAX_FRAME_DATA_LENGTH = 13;
  const static uint8_t AUTOSEND_FRAME_DATA_LENGTH = 32;

  // Byte index in frame data section
  enum DataByteT {
    DATA_DF1 = 0,
    DATA_DF2,
    DATA_DF3,
    DATA_DF4,
    DATA_DF5,
    DATA_DF6,
    DATA_DF7,
    DATA_DF8,
    DATA_DF9,
    DATA_DF10,
    DATA_DF11,
    DATA_DF12,
  };

  // Frame structure, for both request and reply with data

#pragma pack(push, 1)  // Structure alignment on byte boundary
  union Frame_Union {
    // Frame structure
    struct {
      FrameType_t type;
      uint8_t length;
      Cmd_t command;
      uint8_t data[MAX_FRAME_DATA_LENGTH];
    };

    // Reply code on two bytes in case of error or reply without data
    uint16_t code;

    // Raw data
    uint8_t bytes[MAX_FRAME_DATA_LENGTH + 4];
  };

  // Autosend frame for documentation purpose only, not used in this module
  union AutosendFrame_Union {
    // Autosend frame structure
    struct {
      uint16_t header;       // Header, should be 0x424d
      uint16_t length;       // Frame length = 2 x 16 + 2 * (data length + checksum length)
      uint16_t pm_1_0;       // PM1.0 concentration (standard particulate matter)
      uint16_t pm_2_5;       // PM2.5 concentration (standard particulate matter)
      uint16_t pm_4_0;       // PM4.0 concentration (standard particulate matter)
      uint16_t pm_10_0;      // PM10.0 concentration (standard particulate matter)
      uint16_t reserved[9];  // Reserved Data4 to Data12
      uint16_t crc;          // Checksum
    };

    // Raw data
    uint8_t bytes[AUTOSEND_FRAME_DATA_LENGTH];
  };
#pragma pack(pop)

  using Frame_t = union Frame_Union;
  using AutosendFrame_t = union AutosendFrame_Union;

  // Protected member variables
  Error_t last_error_ = ERROR_SUCCESS;
  bool active_ = false;
  uint16_t activation_attempts_ = 0;
  bool adjustment_coefficient_needs_refresh_ = true;

#ifdef USE_NUMBER
  number::Number *adjustment_coefficient_number_{nullptr};
#endif

#ifdef USE_BUTTON
  button::Button *factory_default_button_{nullptr};
#endif

  // Protected member functions
  const float scale_value_(float value, float inMin, float inMax, float outMin, float outMax);
  void print_frame_(const Frame_t frame);
  bool update_frame_crc_(Frame_t &frame);
  bool check_frame_crc_(const Frame_t &frame);
  bool send_request_(Cmd_t command, uint8_t *data, Frame_t &reply);

  void clear_uart_buffer_(void);

  bool read_particle_measuring_results_(float *pm_1_0, float *pm_2_5, float *pm_4_0, float *pm_10_0);
  bool start_particle_measurement_(void);
  bool stop_particle_measurement_(void);
  bool set_customer_adjustment_coefficient_(AdjustmentCoefficient_t new_coefficient);
  bool reset_customer_adjustment_coefficient_(void);
  bool read_customer_adjustment_coefficient_(float *value = nullptr);
  bool stop_autosend_(void);
  bool enable_autosend_(void);

  float compute_aqi_pm_2_5_(float value);
  float compute_aqi_pm_10_0_(float value);

  std::vector<Hpma115C0Listener *> listeners_{};
};

}  // namespace hpma115c0
}  // namespace esphome
