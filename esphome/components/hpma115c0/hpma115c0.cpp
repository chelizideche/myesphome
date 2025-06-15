#include "hpma115c0.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace hpma115c0 {

static const char *const TAG = "hpma115c0";

float Hpma115C0PollingComponent::get_setup_priority() const { return setup_priority::BUS; }

void Hpma115C0PollingComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HMPA115C0:");
  ESP_LOGCONFIG(TAG, "Number:");

#ifdef USE_NUMBER
  LOG_NUMBER(TAG, "  Adj. Coef. :", this->adjustment_coefficient_number_);
#endif
}

// Initialize sensor, read initial state
void Hpma115C0PollingComponent::setup() {
  ESP_LOGCONFIG(TAG, "Running setup");

  // Max allowed blocking delay : serial max timeout + 10ms margin
  this->should_warn_of_blocking(MAX_ALLOWED_REPLY_DELAY + 10U);

  // Stop autosend and emptyy uart receive buffer
  this->reset_sensor_mode_();
}

// Update called on regular interval
void Hpma115C0PollingComponent::update() {
  float pm_1_0, pm_2_5, pm_4_0, pm_10_0, aqi_2_5, aqi_10_0;

  if (!read_particle_measuring_results_(&pm_1_0, &pm_2_5, &pm_4_0, &pm_10_0)) {
    // Mark inactive inernally
    this->sensor_active_ = false;

    // Count (re)activation attempts
    this->activation_attempts_ += 1;

    // If too many attempts, mark sensor failed
    if (this->activation_attempts_ > MAX_ACTIVATION_ATTEMPTS) {
      ESP_LOGE(TAG, "Could not (re)activate sensor after %d runs", this->activation_attempts_);
      this->mark_failed("Unable to communicate with sensor.");
      return;
    }

    // Try to reset sensor mode
    this->reset_sensor_mode_();

    ESP_LOGI(TAG, "Run %d, sensor still not active", this->activation_attempts_);
    return;
  }

  // First time sensor becomes active, latch it
  if (!this->sensor_active_) {
    this->sensor_active_ = true;
    this->activation_attempts_ = 0;  // Reset activation attempt counter
    ESP_LOGI(TAG, "Sensor is now active after %d runs", this->activation_attempts_);
  }

  // Compute air quality indexes
  aqi_2_5 = compute_aqi_pm_2_5_(pm_2_5);
  aqi_10_0 = compute_aqi_pm_10_0_(pm_10_0);

  ESP_LOGI(TAG, "pm(µg/m3):  1µm(%.0f), 2.5µm(%.0f), 4µm(%.0f), 10µm(%.0f)", pm_1_0, pm_2_5, pm_4_0, pm_10_0);
  ESP_LOGI(TAG, "aqi(index): 2.5(%.0f), 10(%.0f)", aqi_2_5, aqi_10_0);

  // In case adjustment coefficient has been changed, request refresh in HA
  if (this->adjustment_coefficient_needs_refresh_) {
    float adjustment_coefficient;

    if (read_customer_adjustment_coefficient_(&adjustment_coefficient)) {
      this->adjustment_coefficient_number_->publish_state(adjustment_coefficient);
      this->adjustment_coefficient_needs_refresh_ = false;
    }
  }

  // Call all listeners' callbacks for value updates
  for (auto &listener : this->listeners_)
    listener->on_new_values(pm_1_0, pm_2_5, pm_4_0, pm_10_0, aqi_2_5, aqi_10_0);
}

// Scale a float value
float Hpma115C0PollingComponent::scale_value_(float value, float in_min, float in_max, float out_min, float out_max) {
  if (abs(in_max - in_min) <= 0.0)
    return NAN;
  return (value - in_min) / (in_max - in_min) * (out_max - out_min) + out_min;
}

// Compute and update frame CRC
bool Hpma115C0PollingComponent::update_frame_crc_(StandardFrameT &frame) {
  uint16_t crc;

  // Sanity check on lentgh
  if (frame.length > MAX_FRAME_DATA_LENGTH) {
    ESP_LOGE(TAG, "Invalid length %d for CMD=0x%d", frame.length, frame.command);
    this->hpma_last_error_ = HPMA_ERROR_DATA_LENGTH_TOO_LONG;
    return false;
  }

  crc = frame.type + frame.length + frame.command;
  for (int data_index = 0; data_index < frame.length - 1; data_index++) {
    crc += frame.data[data_index];
  }
  crc = (0x10000UL - crc) & 0xFF;
  frame.data[frame.length - 1] = (uint8_t) crc;

  this->hpma_last_error_ = HPMA_ERROR_SUCCESS;
  return true;
}

// Check frame CRC return computed
bool Hpma115C0PollingComponent::check_frame_crc_(const StandardFrameT &frame) {
  uint16_t crc;

  // Sanity check on lentgh
  if (frame.length > MAX_FRAME_DATA_LENGTH) {
    ESP_LOGE(TAG, "Invalid length %d in reply %d for CMD=%02X", frame.length, frame.type, frame.command);
    this->hpma_last_error_ = HPMA_ERROR_DATA_LENGTH_TOO_LONG;
    return false;
  }

  // Compute CRC
  crc = frame.type + frame.length + frame.command;

  for (int data_index = 0; data_index < frame.length - 1; data_index++) {
    crc += frame.data[data_index];
  }

  crc = (0x10000UL - crc) & 0xFF;

  // Received frame CRC is located on byte after the data
  if (((uint8_t) crc) != frame.data[frame.length - 1]) {
    ESP_LOGE(TAG, "Invalid crc %02X (%02X expected) for reply %02X to CMD=%02X, LEN=%02X", frame.data[frame.length - 1],
             crc, frame.type, frame.command, frame.length);
    this->hpma_last_error_ = HPMA_ERROR_CRC;
  }

  this->hpma_last_error_ = HPMA_ERROR_SUCCESS;
  return true;
}

// Clear UART buffer(s)
void Hpma115C0PollingComponent::reset_sensor_mode_() {
  uint32_t clear_start = millis();
  uint32_t clear_duration, byte_count = 0;

  // Empty UART buffer before sending command
  byte_count = 0;
  while ((clear_duration = (millis() - clear_start)) < MAX_SETUP_TIME) {
    this->read();
    byte_count += 1;
  }
  ESP_LOGD(TAG, "Emptied %d bytes from UART in %d ms", byte_count, clear_duration);

  // Send command to stop autosend (default mode at power on)
  ESP_LOGI(TAG, "Attempting to Stop Autosend");
  if (!this->stop_autosend_()) {
    ESP_LOGW(TAG, "Failed to stop autosend.");
  }

  // Empty buffer again as a precaution for next commands
  byte_count = 0;
  while ((clear_duration = (millis() - clear_start)) < MAX_SETUP_TIME) {
    this->read();
    byte_count += 1;
  }
  ESP_LOGD(TAG, "Emptied %d bytes from UART in %d ms", byte_count, clear_duration);
}

// Print full frame to log (debug level)
// REQUEST : (HEAD-LEN-CMD)[DATA]#CRC
// REPLY :   (HEAD-LEN-CMD)[DATA]#CRC  or Ack code
static const uint16_t MAX_PRINT_BUFFER_LENGTH = 1024;
void Hpma115C0PollingComponent::print_frame_(StandardFrameT frame) {
  char buffer[MAX_PRINT_BUFFER_LENGTH];

  // Sanity check on lentgh
  if (frame.length > MAX_FRAME_DATA_LENGTH) {
    ESP_LOGE(TAG, "Invalid length %d in reply %d for CMD=%02X", frame.length, frame.type, frame.command);
    return;
  }

  if (frame.type == FRAME_TYPE_CMD) {
    sprintf(buffer, "REQUEST --> ");
  } else {
    sprintf(buffer, "REPLY  <--  ");
  };

  sprintf(buffer + strlen(buffer), "%02X-%02X-%02X[", frame.type, frame.length, frame.command);
  int byte_index;

  for (byte_index = 0; byte_index < frame.length - 1; byte_index++) {
    if (byte_index > 0)
      strlcat(buffer, ".", MAX_PRINT_BUFFER_LENGTH);
    sprintf(buffer + strlen(buffer), "%02X", frame.data[byte_index]);
  }
  sprintf(buffer + strlen(buffer), "]#%02X", frame.data[byte_index]);

  ESP_LOGD(TAG, "%s", buffer);
}

// Send a command to the sensor, read and parse reply
bool Hpma115C0PollingComponent::send_request_(HpmaCmdT command, uint8_t *data, StandardFrameT &reply) {
  StandardFrameT request;
  uint8_t expected_reply_data_length = 0;

  // Set frame type for request
  request.type = FRAME_TYPE_CMD;
  request.command = command;

  // Sanity check on command data length
  switch (command) {
    case CMD_START_PARTICLE_MEASUREMENT:
    case CMD_STOP_PARTICLE_MEASUREMENT:
    case CMD_STOP_AUTO_SEND:
    case CMD_ENABLE_AUTO_SEND:
      request.length = 1;
      expected_reply_data_length = 0;
      break;

    case CMD_SET_CUSTOMER_ADJUSTMENT_COEFFICIENT:
      if (data == nullptr) {
        ESP_LOGE(TAG, "Data should not be null for command 0x%02X.", command);
        this->hpma_last_error_ = HPMA_ERROR_NULL_PARAMETER;
        return false;
      }
      request.length = 2;
      memcpy(request.data, data, request.length - 1);
      expected_reply_data_length = 0;
      break;

    case CMD_READ_PARTICLE_MEASURING_RESULTS:
      request.length = 1;
      expected_reply_data_length = 13;
      break;

    case CMD_READ_CUSTOMER_ADJUSTMENT_COEFFICIENT:
      request.length = 1;
      expected_reply_data_length = 2;
      break;

    default:
      ESP_LOGE(TAG, "Unknown command 0x%02X.", request.command);
      this->hpma_last_error_ = HPMA_ERROR_UNKNOWN_COMMAND;
      return false;
  }

  // Compute crc and update request frame
  if (!this->update_frame_crc_(request))
    return false;
  this->print_frame_(request);

  // Send request frame to sensor
  this->write_array(request.bytes, request.length + 3);

  // Wait until first reply byte becomes available, (measured around 27ms)
  uint64_t wait_time, wait_start = millis();
  while (true) {
    wait_time = millis() - wait_start;
    if (this->available() > 0) {
      ESP_LOGD(TAG, "response time is %llu ms", wait_time);
      break;
    };

    if (wait_time > MAX_ALLOWED_REPLY_DELAY) {
      ESP_LOGE(TAG, "Communication timeout, reply took more than %d ms", MAX_ALLOWED_REPLY_DELAY);
      this->hpma_last_error_ = HPMA_ERROR_TIMEOUT;
      return false;
    }
  }

  // Read first two bytes of response
  if (!this->read_array(reply.bytes, 2)) {
    ESP_LOGE(TAG, "Read timeout on first two response bytes.");
    this->hpma_last_error_ = HPMA_ERROR_TIMEOUT;
    return false;
  }

  // Negative ack from sensor
  if (reply.code == RESPONSE_NEG_ACK) {
    ESP_LOGD(TAG, "REPLY  <--  0x%04X (Neg ACK)", reply.code);
    ESP_LOGE(TAG, "Got negative ack from sensor");
    this->hpma_last_error_ = HPMA_ERROR_NEGATIVE_ACK;
    return false;
  }

  // Positive ack from sensor
  if (reply.code == RESPONSE_POS_ACK) {
    ESP_LOGD(TAG, "REPLY  <--  0x%04X (Pos ACK)", reply.code);
    this->hpma_last_error_ = HPMA_ERROR_SUCCESS;
    return true;
  }

  // Unexpected value for first reply byte
  if (reply.type != FRAME_TYPE_REPLY_WITH_DATA) {
    ESP_LOGE(TAG, "Got unexpected reply from sensor: 0x%02X.", reply.type);
    this->hpma_last_error_ = HPMA_ERROR_UNEXPECTED_REPLY;
    return false;
  }

  // Sanity check on length
  if (reply.length > MAX_FRAME_DATA_LENGTH) {
    ESP_LOGE(TAG, "Got too long reply from sensor: %d bytes.", reply.length);
    this->hpma_last_error_ = HPMA_ERROR_DATA_LENGTH_TOO_LONG;
    return false;
  }

  // Read rest of reply
  if (!this->read_array(reply.bytes + 2, reply.length + 1)) {
    ESP_LOGE(TAG, "Read timeout while reading remaining %d bytes from reply.", reply.length + 2);
    this->hpma_last_error_ = HPMA_ERROR_TIMEOUT;
    return false;
  }

  // Check matching command in reply
  if (reply.command != request.command) {
    ESP_LOGE(TAG, "Command 0x%02X in reply does not match command 0x%02X in request .", reply.command, request.command);
    this->hpma_last_error_ = HPMA_ERROR_COMMAND_MISMATCH;
    return false;
  }

  // Check length consistency
  if (reply.length != expected_reply_data_length) {
    ESP_LOGE(TAG, "Invalid reply data length %d in command 0x%02X, should have been %d.", reply.length, command,
             expected_reply_data_length);
    this->hpma_last_error_ = HPMA_ERROR_INVALID_REPLY_LENGTH;
    return false;
  }

  // Print frame (debug) and check crc
  this->print_frame_(reply);
  if (!this->check_frame_crc_(reply))
    return false;

  this->hpma_last_error_ = HPMA_ERROR_SUCCESS;
  return true;
}

// Read current particulate mater values from sensor
bool Hpma115C0PollingComponent::read_particle_measuring_results_(float *pm_1_0, float *pm_2_5, float *pm_4_0,
                                                                 float *pm_10_0) {
  StandardFrameT reply;

  if (!this->send_request_(CMD_READ_PARTICLE_MEASURING_RESULTS, nullptr, reply)) {
    return false;
  }

  // Update local values
  *pm_1_0 = (float) (reply.data[DATA_DF1] * 256 + reply.data[DATA_DF2]);
  *pm_2_5 = (float) (reply.data[DATA_DF3] * 256 + reply.data[DATA_DF4]);
  *pm_4_0 = (float) (reply.data[DATA_DF5] * 256 + reply.data[DATA_DF6]);
  *pm_10_0 = (float) (reply.data[DATA_DF7] * 256 + reply.data[DATA_DF8]);

  return true;
}

// Start measurements
bool Hpma115C0PollingComponent::start_particle_measurement_() {
  StandardFrameT reply;

  return this->send_request_(CMD_START_PARTICLE_MEASUREMENT, nullptr, reply);
}

// Stop measurements
bool Hpma115C0PollingComponent::stop_particle_measurement_() {
  StandardFrameT reply;

  return this->send_request_(CMD_STOP_PARTICLE_MEASUREMENT, nullptr, reply);
}

// Set adjustment coefficient
bool Hpma115C0PollingComponent::set_customer_adjustment_coefficient_(AdjustmentCoefficient_t new_coefficient) {
  StandardFrameT reply;

  if ((new_coefficient < ADJUSTMENT_COEFFICIENT_MIN) || (new_coefficient > ADJUSTMENT_COEFFICIENT_MAX)) {
    ESP_LOGE(TAG, "Adjustment coefficient %d not in range [%d, %d]", new_coefficient, ADJUSTMENT_COEFFICIENT_MIN,
             ADJUSTMENT_COEFFICIENT_MAX);
    this->hpma_last_error_ = HPMA_ERROR_COEFFICIENT_OUT_OF_RANGE;
    return false;
  }

  return this->send_request_(CMD_SET_CUSTOMER_ADJUSTMENT_COEFFICIENT, &new_coefficient, reply);
}

// Reset adjustment coefficient to default value
bool Hpma115C0PollingComponent::reset_customer_adjustment_coefficient_() {
  return this->set_customer_adjustment_coefficient_(ADJUSTMENT_COEFFICIENT_DEFAULT);
}

// Get current adjustment coefficient
bool Hpma115C0PollingComponent::read_customer_adjustment_coefficient_(float *value) {
  StandardFrameT reply;

  if (!this->send_request_(CMD_READ_CUSTOMER_ADJUSTMENT_COEFFICIENT, nullptr, reply))
    return false;

  if ((reply.data[DATA_DF1] < ADJUSTMENT_COEFFICIENT_MIN) || (reply.data[DATA_DF1] > ADJUSTMENT_COEFFICIENT_MAX)) {
    ESP_LOGW(TAG, "Adjustment coefficient %d not in range [%d; %d].", reply.data[DATA_DF1], ADJUSTMENT_COEFFICIENT_MIN,
             ADJUSTMENT_COEFFICIENT_MAX);
    this->hpma_last_error_ = HPMA_ERROR_COEFFICIENT_OUT_OF_RANGE;
    return false;
  }

  if (value != nullptr)
    *value = (float) reply.data[DATA_DF1];

  return true;
}

// Stop autosend mode
bool Hpma115C0PollingComponent::stop_autosend_() {
  StandardFrameT reply;

  return this->send_request_(CMD_STOP_AUTO_SEND, nullptr, reply);
}

// Enable autosend mode
bool Hpma115C0PollingComponent::enable_autosend_() {
  StandardFrameT reply;

  return this->send_request_(CMD_ENABLE_AUTO_SEND, nullptr, reply);
}

// Computation of AQI PM 2.5 according to airnow.gov
// See https://document.airnow.gov/technical-assistance-document-for-the-reporting-of-daily-air-quailty.pdf
float Hpma115C0PollingComponent::compute_aqi_pm_2_5_(float value) {
  if (value < 0.0)
    return -1.0;
  if (value < 9.1)
    return this->scale_value_(value, 0.0, 9.0, 0, 50);
  if (value < 35.5)
    return this->scale_value_(value, 9.1, 35.4, 51, 100);
  if (value < 55.5)
    return this->scale_value_(value, 35.5, 55.4, 101, 150);
  if (value < 125.5)
    return this->scale_value_(value, 55.5, 125.4, 151, 200);
  if (value < 225.5)
    return this->scale_value_(value, 125.5, 225.4, 201, 300);
  return this->scale_value_(value, 225.5, (float) MAX_PM_CONCENTRATION, 301, MAX_PM_CONCENTRATION);
}

// Computation of AQI PM 10 according to airnow.gov
// See https://document.airnow.gov/technical-assistance-document-for-the-reporting-of-daily-air-quailty.pdf
float Hpma115C0PollingComponent::compute_aqi_pm_10_0_(float value) {
  if (value < 0.0)
    return -1.0;
  if (value < 55)
    return this->scale_value_(value, 0.0, 54, 0, 50);
  if (value < 155)
    return this->scale_value_(value, 55, 154, 51, 100);
  if (value < 255)
    return this->scale_value_(value, 155, 254, 101, 150);
  if (value < 355)
    return this->scale_value_(value, 255, 354, 151, 200);
  if (value < 425)
    return this->scale_value_(value, 355, 424, 201, 300);
  return this->scale_value_(value, 425, (float) MAX_PM_CONCENTRATION, 301, MAX_PM_CONCENTRATION);
}

}  // namespace hpma115c0
}  // namespace esphome
