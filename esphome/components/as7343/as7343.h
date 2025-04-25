#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

#include "as7343_registers.h"

namespace esphome {
namespace as7343 {

class AS7343Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;
  void loop() override;

  void set_gain(AS7343Gain gain) { gain_ = gain; }
  void set_atime(uint8_t atime) { atime_ = atime; }
  void set_astep(uint16_t astep) { astep_ = astep; }
  void set_glass_attenuation_factor(float factor) { this->glass_attenuation_factor_ = factor; }

  AS7343Gain get_gain();
  uint8_t get_atime();
  uint16_t get_astep();

  bool setup_gain(AS7343Gain gain);
  bool setup_atime(uint8_t atime);
  bool setup_astep(uint16_t astep);

  bool change_gain(AS7343Gain gain);

  float get_gain_multiplier(AS7343Gain gain);

  bool read_all_channels();

  bool is_data_ready();

  bool enable_power(bool enable);
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
    COLLECTING_DATA,
    COLLECTING_DATA_AUTO,
    DATA_COLLECTED,
    READY_TO_PUBLISH_PART_1,
    READY_TO_PUBLISH_PART_2,
    READY_TO_PUBLISH_PART_3
  } state_{State::NOT_INITIALIZED};

  void set_bank_for_reg_(AS7343Registers reg = AS7343Registers::ENABLE);
  bool bank_{false};

  //#ifdef USE_SENSOR
  SUB_SENSOR(f1);
  SUB_SENSOR(f2);
  SUB_SENSOR(fz);
  SUB_SENSOR(f3);
  SUB_SENSOR(f4);
  SUB_SENSOR(fy);
  SUB_SENSOR(f5);
  SUB_SENSOR(fxl);
  SUB_SENSOR(f6);
  SUB_SENSOR(f7);
  SUB_SENSOR(f8);
  SUB_SENSOR(nir);
  SUB_SENSOR(clear);
  SUB_SENSOR(illuminance);
  SUB_SENSOR(irradiance);
  SUB_SENSOR(irradiance_photopic);
  SUB_SENSOR(ppfd);
  SUB_SENSOR(par);
  SUB_SENSOR(ct);
  SUB_SENSOR(color_temperature);

 protected:
  //#endif

  sensor::Sensor *saturated_{nullptr};

  uint16_t astep_;
  AS7343Gain gain_;
  uint8_t atime_;

  float glass_attenuation_factor_{1.0f};
  float corr_lx_y_cie1931_{683.0f};

  bool auto_gain_enabled_{true};
  bool continuous_mode_{false};
  // enum : uint8_t {
  //   AS7343_LIGHT_DETECTION = 0,
  //   AS7343_COLOR_DETECTION = 1,
  // } detection_mode_{AS7343_LIGHT_DETECTION};

  struct {
    std::array<uint16_t, AS7343_NUM_CHANNELS + 1> raw_counts{};  // extra channel for safe inverse mapping

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
    std::array<float, AS7343_NUM_CHANNELS + 1> basic_counts{};  // extra channel for safe inverse mapping
    float lux;
    float irradiance;
    float irradiance_photopic;
    float ppfd;
    float par;
    float cct;
    float duv;
    float lux_from_xyz;
  } calculated_values_;

  enum {
    GAIN_OK = 0,
    GAIN_TOO_LOW = 1,
    GAIN_TOO_HIGH = 2,
  } auto_gain_status_{GAIN_OK},
      auto_gain_status_prev_{GAIN_OK};
  uint8_t auto_gain_tries_{0};

  float get_tint_();
  void optimizer_(float max_TINT);
  void direct_config_3_chain_();
  void setup_tint_(float tint);

  bool spectral_post_process_(bool fire_at_will = true);
  void get_optimized_gain_(uint16_t maximum_adc, uint16_t highest_adc, uint8_t lower_gain_limit,
                           uint8_t upper_gain_limit, uint8_t &out_gain, bool &out_saturation);

  uint16_t get_maximum_spectral_adc_();
  uint16_t get_maximum_spectral_adc_(uint16_t atime, uint16_t astep);

  template<typename T, size_t N> T get_highest_value(std::array<T, N> &data);

  bool check_if_adjustments_needed_();
  void calculate_();
  void calculate_basic_counts_();
  void calculate_spectral_(float &lux, float &par, float &ppfd, float &irradiance, float &irradiance_photopic);
  void calculate_color_(float &ct, float &duv, float &lux);

  void calculate_cri_();

  void publish_channel_readings_();
  void publish_derived_readings_();

 public:
  bool as7352_set_integration_time_us(uint32_t time_us);
};

}  // namespace as7343
}  // namespace esphome
