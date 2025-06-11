#include "sc01_o3.h"

static const char *const TAG = "SC01O3Sensor";

namespace esphome {
namespace sc01_o3 {

// The position of various data points in the return packet, excluding the first 0xFF byte.
constexpr uint8_t GAS_NAME_POS = 0;
constexpr uint8_t UNIT_POS = 1;
constexpr uint8_t GAS_CONCENTRATION_HIGH_POS = 3;
constexpr uint8_t GAS_CONCENTRATION_LOW_POS = 4;
constexpr uint8_t CHECKSUM_POS = 7;

// Defined constant
constexpr uint8_t O3_GAS_TYPE = 0x17;
constexpr uint8_t UNIT_PPB = 0x04;
constexpr uint8_t START_BYTE_VALUE = 0xFF;

constexpr std::array<uint8_t, 7> SET_QNA_MODE_COMMAND = {
    0x01,                    // Reserved
    0x78,                    // Change order
    0x41,                    // Q & A mode (This is where we have to ask the device for a
                             // measurement, instead of the measurement happening every second.)
    0x00, 0x00, 0x00, 0x00,  // Reserved
};

constexpr std::array<uint8_t, 7> ASK_FOR_DATA_COMMAND = {
    0x01,                          // Reserved
    0x86,                          // Order
    0x00, 0x00, 0x00, 0x00, 0x00,  // Reserved
};

constexpr std::array<uint8_t, 1> DATA_PREPEND_BYTES = {
    START_BYTE_VALUE,  // Start byte
};

void SC01O3Sensor::setup() {
  if (!this->is_initialized_) {
    this->set_qna_mode_();
  }
}

void SC01O3Sensor::dump_config() { ESP_LOGCONFIG(TAG, "SC01 O3 sensor!"); }

uint8_t SC01O3Sensor::calculate_checksum(const uint8_t *data, uint8_t len) {
  // Calculation is to add up all bytes (except first 0xFF) bytes, then negate it, and add one.
  uint8_t out = 0;
  for (uint8_t i = 0; i < len; ++i) {
    out += data[i];
  }
  out = (~out) + 1;
  return out;
}

void SC01O3Sensor::send_command_(const std::array<uint8_t, 7> data) {
  this->write_array(DATA_PREPEND_BYTES);
  this->write_array(data);
  uint8_t checksum = calculate_checksum(data.data(), data.size());
  this->write_array(&checksum, 1);
}

void SC01O3Sensor::set_qna_mode_() {
  // Called once during setup
  send_command_(SET_QNA_MODE_COMMAND);
  ESP_LOGI(TAG, "O3 Sensor setup complete.");
  this->is_initialized_ = true;
}

void SC01O3Sensor::update() {
  if (!this->is_initialized_) {
    set_qna_mode_();
  }
  ESP_LOGD(TAG, "Update....");
  send_command_(SET_QNA_MODE_COMMAND);
  send_command_(ASK_FOR_DATA_COMMAND);
}

void SC01O3Sensor::loop() {
  // ESP_LOGI(TAG, "Loop....");
  if (!this->is_initialized_) {
    set_qna_mode_();
  }
  // Polling function to read data from UART
  while (this->available() >= 9) {
    // Read the first byte and check for the start byte (0xFF)
    uint8_t first_byte = this->read();
    if (first_byte != START_BYTE_VALUE) {
      continue;  // Skip and search for the next start byte
    }
    // If we find START_BYTE_VALUE, attempt to read the rest of the packet

    uint8_t buffer[8];  // Excluding the start byte
    this->read_array(buffer, 8);

    ESP_LOGD(TAG, "Received data: %x %x %x %x %x %x %x %x", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4],
             buffer[5], buffer[6], buffer[7]);

    // Validate packet
    if (buffer[GAS_NAME_POS] == O3_GAS_TYPE && buffer[UNIT_POS] == UNIT_PPB &&
        buffer[CHECKSUM_POS] == calculate_checksum(buffer, 7)) {  // The gas name and unit should be consistent.
      uint16_t concentration =
          (static_cast<uint16_t>(buffer[GAS_CONCENTRATION_HIGH_POS]) << 8) | buffer[GAS_CONCENTRATION_LOW_POS];
      state = concentration;
      this->publish_state(concentration);
      ESP_LOGI(TAG, "Valid data: %u ppb", concentration);
    } else {
      ESP_LOGW(TAG, "Invalid data received after start byte");
    }
  }
}

}  // namespace sc01_o3
}  // namespace esphome
