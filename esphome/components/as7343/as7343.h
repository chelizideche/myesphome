#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "as7343_registers.h"

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace as7343 {

typedef std::array<float, AS7343_NUM_CHANNELS> CalibrationParams;

// 1 extra place for safe inverse map (CLEAR0,CLEAR1)
typedef std::array<uint16_t, AS7343_NUM_CHANNELS + 1> ChannelValuesUint16;
typedef std::array<float, AS7343_NUM_CHANNELS + 1> ChannelValuesFloat;
typedef std::array<const char *, AS7343_NUM_CHANNELS + 1> ChannelNames;

typedef std::array<sensor::Sensor *, AS7343_NUM_CHANNELS> SensorArray;

class AS7343Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;
  void loop() override;

  void set_gain(AS7343Gain gain) { this->gain_ = gain; }
  void set_atime(uint8_t atime) { this->atime_ = atime; }
  void set_astep(uint16_t astep) { this->astep_ = astep; }
  void set_led_enabled(bool enabled) { this->led_enabled_ = enabled; }

  void set_dark_current_calibration(const CalibrationParams &vals);
  void set_channel_correction(const CalibrationParams &vals);
  void set_glass_attenuation_factor(float factor) { this->glass_attenuation_factor_ = factor; }

  void enable_auto_gain(bool enable) { this->auto_gain_enabled_ = enable; }
  void enable_continuous_mode(bool enable) { this->continuous_mode_ = enable; }
  void enable_scaling(bool enable) { this->scaling_enabled_ = enable; }

  AS7343Gain get_gain();
  uint8_t get_atime();
  uint16_t get_astep();

  bool setup_gain(AS7343Gain gain);
  bool setup_atime(uint8_t atime);
  bool setup_astep(uint16_t astep);

  bool change_gain(AS7343Gain gain);

  float get_gain_multiplier(AS7343Gain gain);

  bool read_and_discard_channels_();
  bool read_channels_();

  bool is_data_ready();

  bool enable_power(bool enable);
  bool enable_led(bool enable);
  bool enable_spectral_measurement(bool enable);

  bool read_register_bit(uint8_t address, uint8_t bit_position);
  bool write_register_bit(uint8_t address, bool value, uint8_t bit_position);
  bool set_register_bit(uint8_t address, uint8_t bit_position);
  bool clear_register_bit(uint8_t address, uint8_t bit_position);
  uint16_t swap_bytes(uint16_t data);

 protected:
  //
  // Internal state machine, used to split all the actions into
  // small steps in loop() to make sure we are not blocking execution
  //
  enum class State : uint8_t {
    NOT_INITIALIZED,
    INITIAL_SETUP_COMPLETED,
    IDLE,
    START_MEASUREMENT,
    WAIT_FOR_DATA,
    READ_CHANNELS,
    // COLLECTING_DATA,
    // COLLECTING_DATA_AUTO,
    DATA_COLLECTED,
    CALCULATE_CIE,
    CALCULATE_SPECTRAL,
    VALIDATE_VALUES,
    READY_TO_PUBLISH_PART_1,
    READY_TO_PUBLISH_PART_2,
    READY_TO_PUBLISH_PART_3
  } state_{State::NOT_INITIALIZED};

  void set_bank_for_reg_(AS7343Registers reg = AS7343Registers::ENABLE);
  bool bank_{false};

#ifdef USE_SENSOR
 protected:
  SensorArray band_counts_sensors_{};
  SensorArray band_basic_counts_sensors_{};

 public:
  void set_band_counts_sensor(sensor::Sensor *sensor, uint8_t channel) {
    if (channel < AS7343_NUM_CHANNELS) {
      band_counts_sensors_[channel] = sensor;
    }
  }
  void set_band_basic_counts_sensor(sensor::Sensor *sensor, uint8_t channel) {
    if (channel < AS7343_NUM_CHANNELS) {
      band_basic_counts_sensors_[channel] = sensor;
    }
  }

  // derived values
  SUB_SENSOR(illuminance);
  SUB_SENSOR(irradiance);
  SUB_SENSOR(irradiance_photopic);
  SUB_SENSOR(irradiance_par);
  SUB_SENSOR(ppfd);
  SUB_SENSOR(ct);
  SUB_SENSOR(color_temperature);
  SUB_SENSOR(saturation_level);
#endif

#ifdef USE_TEXT_SENSOR
 protected:
  text_sensor::TextSensor *rgb_hex_sensor_{nullptr};
  char rgb_hex_str_[10]{0};

 public:
  void set_rgb_hex_sensor(text_sensor::TextSensor *sensor) { this->rgb_hex_sensor_ = sensor; }
#endif

 protected:
  uint16_t astep_;
  AS7343Gain gain_;
  uint8_t atime_;
  bool led_enabled_{false};

  ChannelValuesFloat dark_current_offset_{};
  // from GD
  ChannelValuesFloat channel_correction_{1.055464349, 1.043509797, 1.029576268, 1.0175052,   1.00441899,
                                         0.987356499, 0.957597044, 0.995863485, 1.014628964, 0.996500814,
                                         0.933072749, 1.052236338, 0.999570232};
  float glass_attenuation_factor_{1.0f};
  float corr_lx_y_cie1931_{683.002f};

  bool scaling_enabled_{false};

  bool auto_gain_enabled_{false};
  bool continuous_mode_{false};

  struct {
    ChannelValuesUint16 raw_counts{};

    bool saturated{false};
    AS7343Gain gain;
    uint8_t atime;
    uint16_t astep;

    float gain_x;
    float t_int_us;
    uint32_t millis_start;

    bool first_run{true};
    bool valid{false};
  } readings_;

  struct {
    ChannelValuesFloat basic_counts{};
    float max_basic_count;

    float lux_from_irradiance;
    float irradiance;
    float irradiance_photopic;
    float irradiance_par;
    float ppfd;
    float cct;
    float duv;
    float lux_from_xyz;
    float lux;
    float saturation_level;
  } calculated_values_;

  void calculate_();
  void calculate_basic_counts_();
  void calculate_spectral_(float &lux, float &par, float &ppfd, float &irradiance, float &irradiance_photopic);
  void calculate_color_(float &ct, float &duv, float &lux);
  void calculate_cri_();

  void publish_channel_readings_();
  void publish_basic_counts_();
  void publish_derived_readings_();

  // auto gain section
  enum {
    GAIN_OK = 0,
    GAIN_TOO_LOW = 1,
    GAIN_TOO_HIGH = 2,
  } auto_gain_status_{GAIN_OK},
      auto_gain_status_prev_{GAIN_OK};
  uint8_t auto_gain_tries_{0};

  float get_tint_();
  uint16_t get_maximum_spectral_adc_();
  uint16_t get_maximum_spectral_adc_(uint16_t atime, uint16_t astep);

  template<typename T, size_t N> T get_highest_value(std::array<T, N> &data);
  void optimizer_(float max_TINT);
  void direct_config_3_chain_();
  void setup_tint_(float tint);

  bool spectral_post_process_(bool fire_at_will = true);
  void get_optimized_gain_(uint16_t maximum_adc, uint16_t highest_adc, uint8_t lower_gain_limit,
                           uint8_t upper_gain_limit, uint8_t &out_gain, bool &out_saturation);

  bool check_if_adjustments_needed_();

 public:
  bool as7352_set_integration_time_us(uint32_t time_us);

#ifdef USE_SENSOR
  void publish_sensor(sensor::Sensor *sensor, float value) {
    if (sensor != nullptr) {
      sensor->publish_state(value);
    }
  }
#endif
};

}  // namespace as7343
}  // namespace esphome
