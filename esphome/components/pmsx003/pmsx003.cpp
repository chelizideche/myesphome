#include "pmsx003.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace pmsx003 {

static const char *const TAG = "pmsx003";

static const uint8_t START_CHARACTER_1 = 0x42;
static const uint8_t START_CHARACTER_2 = 0x4D;

// Time until measurement data is stable (after power on).
static const uint16_t PMS_STABILISING_MS = 30000;
// Maximum expected time for receiving data
static const uint16_t PMS_RECEIVE_TIMEOUT_MS = 500;

static const uint16_t PMS_CMD_MEASUREMENT_MODE_PASSIVE =
    0x0000;  // use `PMS_CMD_MANUAL_MEASUREMENT` to trigger a measurement
static const uint16_t PMS_CMD_MEASUREMENT_MODE_ACTIVE = 0x0001;  // automatically perform measurements
static const uint16_t PMS_CMD_SLEEP_MODE_SLEEP = 0x0000;         // go to sleep mode
static const uint16_t PMS_CMD_SLEEP_MODE_WAKEUP = 0x0001;        // wake up from sleep mode

void PMSX003Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PMSX003:");
  LOG_SENSOR("  ", "PM1.0STD", this->pm_1_0_std_sensor_);
  LOG_SENSOR("  ", "PM2.5STD", this->pm_2_5_std_sensor_);
  LOG_SENSOR("  ", "PM10.0STD", this->pm_10_0_std_sensor_);

  LOG_SENSOR("  ", "PM1.0", this->pm_1_0_sensor_);
  LOG_SENSOR("  ", "PM2.5", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PM10.0", this->pm_10_0_sensor_);

  LOG_SENSOR("  ", "PM0.3um", this->pm_particles_03um_sensor_);
  LOG_SENSOR("  ", "PM0.5um", this->pm_particles_05um_sensor_);
  LOG_SENSOR("  ", "PM1.0um", this->pm_particles_10um_sensor_);
  LOG_SENSOR("  ", "PM2.5um", this->pm_particles_25um_sensor_);
  LOG_SENSOR("  ", "PM5.0um", this->pm_particles_50um_sensor_);
  LOG_SENSOR("  ", "PM10.0um", this->pm_particles_100um_sensor_);

  LOG_SENSOR("  ", "Formaldehyde", this->formaldehyde_sensor_);

  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  this->check_uart_settings(9600);
}

void PMSX003Component::setup() {
  ESP_LOGCONFIG(TAG, "PMSX003 setup:");

  // If the update inverval is longer than the stabiliztion
  // time, then put the device in sleep mode between
  // measurements.

  ESP_LOGD(TAG, "PMSX003 update_interval: %u", this->update_interval_);

  if (this->update_interval_ < PMS_STABILISING_MS) {
    // Send active-measurement command, then wait until we get
    // stable data and start reporting.
    ESP_LOGD(TAG, "Using active measurement");
    this->send_command_(PMS_CMD_MEASUREMENT_MODE, PMS_CMD_MEASUREMENT_MODE_ACTIVE);
    this->set_timeout("PMSX003 measure active", PMS_STABILISING_MS, [this]() { this->measure_active_initial_(); });
  } else {
    // Send passive-measurement command, then start the
    // wakeup-stabelize-measure-sleep cycle.
    ESP_LOGD(TAG, "Using passive measurement");
    this->send_command_(PMS_CMD_MEASUREMENT_MODE, PMS_CMD_MEASUREMENT_MODE_PASSIVE);
    this->wakeup_device_();
  }
}

void PMSX003Component::measure_active_initial_() {
  // The device sends invalid data during the stabilization period.
  // Let's drop this data now and process all data from now on.
  while (this->available())
    this->read_byte(&this->data_[0]);

  // Schedule new data processing.
  uint32_t timeout_ms = PMS_RECEIVE_TIMEOUT_MS;
  this->set_timeout("PMSX003 update", timeout_ms, [this]() { this->measure_active_(); });
}

void PMSX003Component::measure_active_() {
  // Process received data.
  this->process_data_();

  // Schedule new data processing.
  uint32_t timeout_ms = PMS_RECEIVE_TIMEOUT_MS;
  this->set_timeout("PMSX003 update", timeout_ms, [this]() { this->measure_active_(); });
}

void PMSX003Component::wakeup_device_() {
  // Wake the device and wait for stable data.
  this->send_command_(PMS_CMD_SLEEP_MODE, PMS_CMD_SLEEP_MODE_WAKEUP);

  uint32_t timeout_ms = PMS_STABILISING_MS;
  this->set_timeout("PMSX003 stabelize", timeout_ms, [this]() { this->request_data_(); });
}

void PMSX003Component::request_data_() {
  // Drop all data that we received so far.
  while (this->available())
    this->read_byte(&this->data_[0]);

  // Request new data.
  this->send_command_(PMS_CMD_MANUAL_MEASUREMENT, 0);

  // Wait a bit until we receive data.
  uint32_t timeout_ms = PMS_RECEIVE_TIMEOUT_MS;
  this->set_timeout("PMSX003 receive", timeout_ms, [this]() { this->measure_passive_(); });
}

void PMSX003Component::measure_passive_() {
  // Process one frame. The restriction to one comes from the fact
  // that the device sends two identical. The data in the input buffer
  // will be handled in the next call of `request_data()`.
  this->process_data_(1);

  // Put the device in sleep mode.
  this->send_command_(PMS_CMD_SLEEP_MODE, PMS_CMD_SLEEP_MODE_SLEEP);

  // Set callback to wake up the device.
  assert(this->update_interval_ >= PMS_STABILISING_MS);
  uint32_t timeout_ms = this->update_interval_ - PMS_STABILISING_MS;
  this->set_timeout("PMSX003 sleep", timeout_ms, [this]() { this->wakeup_device_(); });
}

inline void PMSX003Component::clear_data_buffer_() {
  memset(this->data_, 0, this->data_index_);
  this->data_index_ = 0;
}

void PMSX003Component::process_data_(unsigned n) {
  // We process all data in the receive buffer.
  // Each byte will be read into a processing buffer and checked.
  // Once we see a complete frame, we parse the data and publish them.
  // Note that we might have received multiple frames.
  unsigned frames = 0;
  while (this->available()) {
    this->read_byte(&this->data_[this->data_index_]);
    PMSX003ParserState state = this->check_byte_();
    switch (state) {
      case PMSX003_PARSER_MORE:
        this->data_index_++;
        break;
      case PMSX003_PARSER_COMPLETE:
        this->parse_data_();
        clear_data_buffer_();
        frames++;
        if (n && frames >= n)
          return;
        break;
      case PMSX003_PARSER_ERROR:
        clear_data_buffer_();
        break;
    }
  }
}

PMSX003ParserState PMSX003Component::check_byte_() {
  const uint8_t index = this->data_index_;
  const uint8_t byte = this->data_[index];

  if (index == 0 || index == 1) {
    const uint8_t start_char = index == 0 ? START_CHARACTER_1 : START_CHARACTER_2;
    if (byte == start_char) {
      return PMSX003_PARSER_MORE;
    }

    ESP_LOGW(TAG, "Start character %u mismatch: 0x%02X != 0x%02X", index + 1, byte, START_CHARACTER_1);
    return PMSX003_PARSER_ERROR;
  }

  if (index == 2) {
    return PMSX003_PARSER_MORE;
  }

  const uint16_t payload_length = this->get_16_bit_uint_(2);
  if (index == 3) {
    if (this->check_payload_length_(payload_length)) {
      return PMSX003_PARSER_MORE;
    } else {
      ESP_LOGW(TAG, "Payload length %u doesn't match. Are you using the correct PMSX003 type?", payload_length);
      return PMSX003_PARSER_ERROR;
    }
  }

  // start (16bit) + length (16bit) + DATA (payload_length - 16bit) + checksum (16bit)
  const uint16_t total_size = 4 + payload_length;

  if (index < total_size - 1) {
    return PMSX003_PARSER_MORE;
  }

  // checksum is without checksum bytes
  uint16_t checksum = 0;
  for (uint16_t i = 0; i < total_size - 2; i++) {
    checksum += this->data_[i];
  }

  const uint16_t check = this->get_16_bit_uint_(total_size - 2);
  if (checksum != check) {
    ESP_LOGW(TAG, "PMSX003 checksum mismatch! 0x%02X != 0x%02X", checksum, check);
    return PMSX003_PARSER_ERROR;
  }

  return PMSX003_PARSER_COMPLETE;
}

bool PMSX003Component::check_payload_length_(uint16_t payload_length) {
  switch (this->type_) {
    case PMSX003_TYPE_X003:
      // The expected payload length is typically 28 bytes.
      // However, a 20-byte payload check was already present in the code.
      // No official documentation was found confirming this.
      // Retaining this check to avoid breaking existing behavior.
      return payload_length == 28 || payload_length == 20;  // 2*13+2
    case PMSX003_TYPE_5003T:
    case PMSX003_TYPE_5003S:
      return payload_length == 28;  // 2*13+2 (Data 13 not set/reserved)
    case PMSX003_TYPE_5003ST:
      return payload_length == 36;  // 2*17+2 (Data 16 not set/reserved)
  }
  return false;
}

void PMSX003Component::parse_data_() {
  // Particle Matter
  const uint16_t pm_1_0_std_concentration = this->get_16_bit_uint_(4);
  const uint16_t pm_2_5_std_concentration = this->get_16_bit_uint_(6);
  const uint16_t pm_10_0_std_concentration = this->get_16_bit_uint_(8);

  const uint16_t pm_1_0_concentration = this->get_16_bit_uint_(10);
  const uint16_t pm_2_5_concentration = this->get_16_bit_uint_(12);
  const uint16_t pm_10_0_concentration = this->get_16_bit_uint_(14);

  const uint16_t pm_particles_03um = this->get_16_bit_uint_(16);
  const uint16_t pm_particles_05um = this->get_16_bit_uint_(18);
  const uint16_t pm_particles_10um = this->get_16_bit_uint_(20);
  const uint16_t pm_particles_25um = this->get_16_bit_uint_(22);

  ESP_LOGD(TAG,
           "Got PM1.0 Standard Concentration: %u µg/m³, PM2.5 Standard Concentration %u µg/m³, PM10.0 Standard "
           "Concentration: %u µg/m³, PM1.0 Concentration: %u µg/m³, PM2.5 Concentration %u µg/m³, PM10.0 "
           "Concentration: %u µg/m³",
           pm_1_0_std_concentration, pm_2_5_std_concentration, pm_10_0_std_concentration, pm_1_0_concentration,
           pm_2_5_concentration, pm_10_0_concentration);

  if (this->pm_1_0_std_sensor_ != nullptr)
    this->pm_1_0_std_sensor_->publish_state(pm_1_0_std_concentration);
  if (this->pm_2_5_std_sensor_ != nullptr)
    this->pm_2_5_std_sensor_->publish_state(pm_2_5_std_concentration);
  if (this->pm_10_0_std_sensor_ != nullptr)
    this->pm_10_0_std_sensor_->publish_state(pm_10_0_std_concentration);

  if (this->pm_1_0_sensor_ != nullptr)
    this->pm_1_0_sensor_->publish_state(pm_1_0_concentration);
  if (this->pm_2_5_sensor_ != nullptr)
    this->pm_2_5_sensor_->publish_state(pm_2_5_concentration);
  if (this->pm_10_0_sensor_ != nullptr)
    this->pm_10_0_sensor_->publish_state(pm_10_0_concentration);

  if (this->pm_particles_03um_sensor_ != nullptr)
    this->pm_particles_03um_sensor_->publish_state(pm_particles_03um);
  if (this->pm_particles_05um_sensor_ != nullptr)
    this->pm_particles_05um_sensor_->publish_state(pm_particles_05um);
  if (this->pm_particles_10um_sensor_ != nullptr)
    this->pm_particles_10um_sensor_->publish_state(pm_particles_10um);
  if (this->pm_particles_25um_sensor_ != nullptr)
    this->pm_particles_25um_sensor_->publish_state(pm_particles_25um);

  if (this->type_ == PMSX003_TYPE_5003T) {
    ESP_LOGD(TAG,
             "Got PM0.3 Particles: %u Count/0.1L, PM0.5 Particles: %u Count/0.1L, PM1.0 Particles: %u Count/0.1L, "
             "PM2.5 Particles %u Count/0.1L",
             pm_particles_03um, pm_particles_05um, pm_particles_10um, pm_particles_25um);
  } else {
    // Note the pm particles 50um & 100um are not returned,
    // as PMS5003T uses those data values for temperature and humidity.
    const uint16_t pm_particles_50um = this->get_16_bit_uint_(24);
    const uint16_t pm_particles_100um = this->get_16_bit_uint_(26);

    ESP_LOGD(TAG,
             "Got PM0.3 Particles: %u Count/0.1L, PM0.5 Particles: %u Count/0.1L, PM1.0 Particles: %u Count/0.1L, "
             "PM2.5 Particles %u Count/0.1L, PM5.0 Particles: %u Count/0.1L, PM10.0 Particles %u Count/0.1L",
             pm_particles_03um, pm_particles_05um, pm_particles_10um, pm_particles_25um, pm_particles_50um,
             pm_particles_100um);

    if (this->pm_particles_50um_sensor_ != nullptr)
      this->pm_particles_50um_sensor_->publish_state(pm_particles_50um);
    if (this->pm_particles_100um_sensor_ != nullptr)
      this->pm_particles_100um_sensor_->publish_state(pm_particles_100um);
  }

  // Formaldehyde
  if (this->type_ == PMSX003_TYPE_5003ST || this->type_ == PMSX003_TYPE_5003S) {
    const uint16_t formaldehyde = this->get_16_bit_uint_(28);

    ESP_LOGD(TAG, "Got Formaldehyde: %u µg/m^3", formaldehyde);

    if (this->formaldehyde_sensor_ != nullptr)
      this->formaldehyde_sensor_->publish_state(formaldehyde);
  }

  // Temperature and Humidity
  if (this->type_ == PMSX003_TYPE_5003ST || this->type_ == PMSX003_TYPE_5003T) {
    const uint8_t temperature_offset = (this->type_ == PMSX003_TYPE_5003T) ? 24 : 30;

    const float temperature = static_cast<int16_t>(this->get_16_bit_uint_(temperature_offset)) / 10.0f;
    const float humidity = this->get_16_bit_uint_(temperature_offset + 2) / 10.0f;

    ESP_LOGD(TAG, "Got Temperature: %.1f°C, Humidity: %.1f%%", temperature, humidity);

    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(temperature);
    if (this->humidity_sensor_ != nullptr)
      this->humidity_sensor_->publish_state(humidity);
  }

  // Firmware Version and Error Code
  if (this->type_ == PMSX003_TYPE_5003ST) {
    const uint8_t firmware_version = this->data_[36];
    const uint8_t error_code = this->data_[37];

    ESP_LOGD(TAG, "Got Firmware Version: 0x%02X, Error Code: 0x%02X", firmware_version, error_code);
  }
}

void PMSX003Component::send_command_(PMSX0003Command cmd, uint16_t data) {
  uint8_t send_data[7] = {
      START_CHARACTER_1,            // Start Byte 1
      START_CHARACTER_2,            // Start Byte 2
      cmd,                          // Command
      uint8_t((data >> 8) & 0xFF),  // Data 1
      uint8_t((data >> 0) & 0xFF),  // Data 2
      0,                            // Verify Byte 1
      0,                            // Verify Byte 2
  };

  // Calculate checksum
  uint16_t checksum = 0;
  for (uint8_t i = 0; i < 5; i++) {
    checksum += send_data[i];
  }
  send_data[5] = (checksum >> 8) & 0xFF;  // Verify Byte 1
  send_data[6] = (checksum >> 0) & 0xFF;  // Verify Byte 2

  for (auto send_byte : send_data) {
    this->write_byte(send_byte);
  }
}

}  // namespace pmsx003
}  // namespace esphome
