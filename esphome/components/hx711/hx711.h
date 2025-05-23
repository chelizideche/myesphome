#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"

#include <cinttypes>

namespace esphome {
namespace hx711 {

enum HX711Gain : uint8_t {
  HX711_GAIN_128 = 1,
  HX711_GAIN_32 = 2,
  HX711_GAIN_64 = 3,
};

class HX711Sensor : public sensor::Sensor, public PollingComponent {
 public:
  void set_dout_pin(GPIOPin *dout_pin) { dout_pin_ = dout_pin; }
  void set_sck_pin(GPIOPin *sck_pin) { sck_pin_ = sck_pin; }
  void set_gain(HX711Gain gain) { gain_ = gain; }
  void set_settling_time(uint16_t settling_time_ms) { this->settling_time_ms_ = settling_time_ms; }
  void set_settle_on_boot(bool settle_on_boot) { this->settle_on_boot_ = settle_on_boot; }

  void call_setup() override;
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;
  /// @brief Returns whether the HX711 ADC has reached a stable state.
  /// @return True if the HX711 ADC has reached a stable state, false otherwise.
  bool is_settled() const { return this->settled_; }

 protected:
  /// @brief Starts the settling timeout sequence for the HX711 sensor.
  ///
  /// Stops the poller and resets the settled state, then initiates a timeout
  /// to allow the HX711 to stabilize after power-up, reset, or gain/channel changes.
  ///
  /// Once the configured settling time has elapsed, marks the sensor as settled,
  /// clears any warning status, and restarts the poller.
  void start_settle_timeoout();

  /// @brief Read sensor data from HX711.
  /// @param[out] result Pointer to store the read value.
  /// @param[in] force Force reading even if settling time has not elapsed.
  /// @return True if data was successfully read, false otherwise.
  bool read_sensor_(uint32_t *result, bool force = false);

  /// @brief Settling time in milliseconds.
  ///
  /// Taken from the datasheet: "Settling time refers to the time from power up,
  /// reset, input channel change and gain change to valid stable output data."
  ///
  /// @note Delta Sigma ADC conversion needs priming, and first four ADC readings
  ///       after the start (400ms at 10 Hz) are usually containing errors and must be discarded.
  uint16_t settling_time_ms_;

  /// @brief Flag to indicate whether the ADC has reached a stable state.
  bool settled_;
  /// @brief Flag to indicate whether the settling has to be done at startup.
  bool settle_on_boot_;

  GPIOPin *dout_pin_;
  GPIOPin *sck_pin_;
  /// Gain to set after new measurement.
  HX711Gain gain_{HX711_GAIN_128};
};

}  // namespace hx711
}  // namespace esphome
