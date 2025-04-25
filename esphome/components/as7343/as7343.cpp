#include "as7343.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#include "as7343_calibration.h"

namespace esphome {
namespace as7343 {

static const char *const TAG = "as7343";

static constexpr uint32_t DATA_COLLECTION_TIMEOUT_MS = 30 * 1000;  // 30 seconds
static constexpr uint8_t MAX_AUTO_GAIN_TRIES = 10 * 2;

//
// SMUX map
static constexpr uint8_t SMUX_CHANNEL_MAP[AS7343_NUM_CHANNELS] = {
    AS7343_CHANNEL_405_F1, AS7343_CHANNEL_425_F2,  AS7343_CHANNEL_450_FZ,  AS7343_CHANNEL_475_F3, AS7343_CHANNEL_515_F4,
    AS7343_CHANNEL_555_FY, AS7343_CHANNEL_550_F5,  AS7343_CHANNEL_600_FXL, AS7343_CHANNEL_640_F6, AS7343_CHANNEL_690_F7,
    AS7343_CHANNEL_745_F8, AS7343_CHANNEL_855_NIR, AS7343_CHANNEL_CLEAR_0};

// Channel name to index mapping
static constexpr uint8_t CHANNEL_IDX[AS7343_NUM_CHANNELS_MAX] = {
    2, 5, 7, 11, AS7343_NUM_CHANNELS, AS7343_NUM_CHANNELS, 1, 3, 4, 8, 12, AS7343_NUM_CHANNELS,
    0, 6, 9, 10, AS7343_NUM_CHANNELS, AS7343_NUM_CHANNELS};

static constexpr std::array<const char *, AS7343_NUM_CHANNELS + 1> CHANNEL_NAME = {
    "F1", "F2", "FZ", "F3", "F4", "FY", "F5", "FXL", "F6", "F7", "F8", "NIR", "Clear", ""};
static constexpr std::array<uint16_t, AS7343_NUM_CHANNELS + 1> CHANNEL_WAVE_NM = {405, 425, 450, 475, 515, 555, 550,
                                                                                  600, 640, 690, 745, 855, 718, 0};

// LOGGING. printing out channel values except for last one which is fake just for the safety of inverse mapping
void log_cn_f(const char *TAG, const char *str, const std::array<float, AS7343_NUM_CHANNELS + 1> &arr) {
  ESP_LOGD(TAG, "%s, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, ", str, arr[0],
           arr[1], arr[2], arr[3], arr[4], arr[5], arr[6], arr[7], arr[8], arr[9], arr[10], arr[11], arr[12]);
}

// LOGGING. printing out channel values except for last one which is fake just for the safety of inverse mapping
void log_cn_f0(const char *TAG, const char *str, const std::array<float, AS7343_NUM_CHANNELS + 1> &arr) {
  ESP_LOGD(TAG, "%s, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, ", str, arr[0],
           arr[1], arr[2], arr[3], arr[4], arr[5], arr[6], arr[7], arr[8], arr[9], arr[10], arr[11], arr[12]);
}

// LOGGING. printing out channel values except for last one which is fake just for the safety of inverse mapping
void log_cn_d(const char *TAG, const char *str, const std::array<uint16_t, AS7343_NUM_CHANNELS + 1> &arr) {
  ESP_LOGD(TAG, "%s, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, ", str, arr[0], arr[1], arr[2], arr[3], arr[4],
           arr[5], arr[6], arr[7], arr[8], arr[9], arr[10], arr[11], arr[12]);
}

// LOGGING. printing out channel values except for last one which is fake just for the safety of inverse mapping
void log_cn_s(const char *TAG, const char *str, const std::array<const char *, AS7343_NUM_CHANNELS + 1> &arr) {
  ESP_LOGD(TAG, "%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, ", str, arr[0], arr[1], arr[2], arr[3], arr[4],
           arr[5], arr[6], arr[7], arr[8], arr[9], arr[10], arr[11], arr[12]);
}

void AS7343Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up AS7343...");
  LOG_I2C_DEVICE(this);

  // Verify device ID
  this->set_bank_for_reg_(AS7343Registers::ID);
  uint8_t id = this->reg((uint8_t) AS7343Registers::ID).get();
  ESP_LOGCONFIG(TAG, "  Read ID: 0x%X", id);
  if (id != AS7343_CHIP_ID) {
    this->mark_failed();
    ESP_LOGE(TAG, "  Invalid chip ID: 0x%X", id);
    return;
  }

  this->set_bank_for_reg_(AS7343Registers::ENABLE);
  // Power on (enter IDLE state)
  if (!this->enable_power(true)) {
    ESP_LOGE(TAG, "  Power on failed!");
    this->mark_failed();
    return;
  }

  // Set configuration
  AS7343RegCfg20 cfg20;
  cfg20.raw = this->reg((uint8_t) AS7343Registers::CFG20).get();
  cfg20.auto_smux = 0b11;
  this->reg((uint8_t) AS7343Registers::CFG20) = cfg20.raw;

  this->direct_config_3_chain_();

  this->setup_atime(this->atime_);
  this->setup_astep(this->astep_);
  this->setup_gain(this->gain_);

  // enable led false ?

  this->enable_spectral_measurement(true);

  this->state_ = State::IDLE;
}

void AS7343Component::dump_config() {
  ESP_LOGCONFIG(TAG, "AS7343:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with AS7343 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Gain: %.1f", get_gain_multiplier(get_gain()));
  ESP_LOGCONFIG(TAG, "  ATIME: %u", get_atime());
  ESP_LOGCONFIG(TAG, "  ASTEP: %u", get_astep());
  ESP_LOGCONFIG(TAG, "  Glass attenuation factor: %f", this->glass_attenuation_factor_);

  LOG_SENSOR("  ", "F1", this->f1_sensor_);
  LOG_SENSOR("  ", "F2", this->f2_sensor_);
  LOG_SENSOR("  ", "FZ", this->fz_sensor_);
  LOG_SENSOR("  ", "F3", this->f3_sensor_);
  LOG_SENSOR("  ", "F4", this->f4_sensor_);
  LOG_SENSOR("  ", "FY", this->fy_sensor_);
  LOG_SENSOR("  ", "F5", this->f5_sensor_);
  LOG_SENSOR("  ", "FXL", this->fxl_sensor_);
  LOG_SENSOR("  ", "F6", this->f6_sensor_);
  LOG_SENSOR("  ", "F7", this->f7_sensor_);
  LOG_SENSOR("  ", "F8", this->f8_sensor_);
  LOG_SENSOR("  ", "NIR", this->nir_sensor_);
  LOG_SENSOR("  ", "Clear", this->clear_sensor_);
}

float AS7343Component::get_setup_priority() const { return setup_priority::DATA; }

void AS7343Component::update() {
  if (this->is_ready() && this->state_ == State::IDLE) {
    ESP_LOGV(TAG, "Initiating new data collection");
    this->state_ = State::START_MEASUREMENT;
  } else {
    ESP_LOGV(TAG, "Can't initiate new data collection - component not ready");
  }
}

void AS7343Component::loop() {
  if (this->is_ready()) {
    // pre-read all channels so values are up to date
    // readings are valid when 1) valid flag is set and 2) all channels are read
    if (this->continuous_mode_ && this->is_data_ready()) {
      this->readings_.valid = this->read_all_channels();
    }

    switch (this->state_) {
      case State::IDLE:
        if (this->auto_gain_enabled_ && this->readings_.valid) {
          this->check_if_adjustments_needed_();
        }
        break;

      case State::START_MEASUREMENT:
        // start measurement
        this->readings_.millis_start = millis();
        // this->enable_spectral_measurement(true); // it clears valid flag
        this->state_ = State::COLLECTING_DATA;
        break;

      case State::COLLECTING_DATA:
        if (!this->continuous_mode_ && this->is_data_ready()) {
          this->readings_.valid = this->read_all_channels();
        }
        if (this->readings_.valid) {
          this->enable_spectral_measurement(true);  // keep measurements on
          if (this->readings_.first_run) {
            this->readings_.first_run = false;
            this->state_ = State::START_MEASUREMENT;
          } else {
            this->state_ = State::DATA_COLLECTED;
          }
        } else if (millis() - this->readings_.millis_start > DATA_COLLECTION_TIMEOUT_MS) {
          ESP_LOGW(TAG, "Data collection timeout");
          this->state_ = State::IDLE;
        }
        break;

      case State::DATA_COLLECTED:
        if (this->auto_gain_enabled_ && this->check_if_adjustments_needed_()) {
          this->state_ = State::COLLECTING_DATA;
        } else {
          // apply modifications
          this->calculate_();
          this->state_ = State::READY_TO_PUBLISH_PART_1;
        }
        break;

      case State::READY_TO_PUBLISH_PART_1:
        // publish
        this->publish_channel_readings_();
        this->state_ = State::READY_TO_PUBLISH_PART_2;
        break;

      case State::READY_TO_PUBLISH_PART_2:
        this->publish_derived_readings_();
        this->state_ = State::READY_TO_PUBLISH_PART_3;
        break;

      case State::READY_TO_PUBLISH_PART_3:
        // publish
        // this->calculate_and_publish();
        this->state_ = State::IDLE;
        break;
    }
  }
}

void AS7343Component::publish_channel_readings_() {
  if (this->f1_sensor_ != nullptr) {
    this->f1_sensor_->publish_state(this->calculated_values_.basic_counts[CHANNEL_IDX[AS7343_CHANNEL_405_F1]]);
  }
  if (this->f2_sensor_ != nullptr) {
    this->f2_sensor_->publish_state(this->calculated_values_.basic_counts[CHANNEL_IDX[AS7343_CHANNEL_425_F2]]);
  }
  if (this->fz_sensor_ != nullptr) {
    this->fz_sensor_->publish_state(this->calculated_values_.basic_counts[CHANNEL_IDX[AS7343_CHANNEL_450_FZ]]);
  }
  if (this->f3_sensor_ != nullptr) {
    this->f3_sensor_->publish_state(this->calculated_values_.basic_counts[CHANNEL_IDX[AS7343_CHANNEL_475_F3]]);
  }
  if (this->f4_sensor_ != nullptr) {
    this->f4_sensor_->publish_state(this->calculated_values_.basic_counts[CHANNEL_IDX[AS7343_CHANNEL_515_F4]]);
  }
  if (this->fy_sensor_ != nullptr) {
    this->fy_sensor_->publish_state(this->calculated_values_.basic_counts[CHANNEL_IDX[AS7343_CHANNEL_555_FY]]);
  }
  if (this->f5_sensor_ != nullptr) {
    this->f5_sensor_->publish_state(this->calculated_values_.basic_counts[CHANNEL_IDX[AS7343_CHANNEL_550_F5]]);
  }
  if (this->fxl_sensor_ != nullptr) {
    this->fxl_sensor_->publish_state(this->calculated_values_.basic_counts[CHANNEL_IDX[AS7343_CHANNEL_600_FXL]]);
  }
  if (this->f6_sensor_ != nullptr) {
    this->f6_sensor_->publish_state(this->calculated_values_.basic_counts[CHANNEL_IDX[AS7343_CHANNEL_640_F6]]);
  }
  if (this->f7_sensor_ != nullptr) {
    this->f7_sensor_->publish_state(this->calculated_values_.basic_counts[CHANNEL_IDX[AS7343_CHANNEL_690_F7]]);
  }
  if (this->f8_sensor_ != nullptr) {
    this->f8_sensor_->publish_state(this->calculated_values_.basic_counts[CHANNEL_IDX[AS7343_CHANNEL_745_F8]]);
  }
  if (this->nir_sensor_ != nullptr) {
    this->nir_sensor_->publish_state(this->calculated_values_.basic_counts[CHANNEL_IDX[AS7343_CHANNEL_855_NIR]]);
  }
  if (this->clear_sensor_ != nullptr) {
    this->clear_sensor_->publish_state(this->calculated_values_.basic_counts[CHANNEL_IDX[AS7343_CHANNEL_CLEAR_0]]);
  }
}

void AS7343Component::publish_derived_readings_() {
  if (this->illuminance_sensor_ != nullptr) {
    this->illuminance_sensor_->publish_state(this->calculated_values_.lux);
  }
  if (this->irradiance_sensor_ != nullptr) {
    this->irradiance_sensor_->publish_state(this->calculated_values_.irradiance);
  }
  if (this->irradiance_photopic_sensor_ != nullptr) {
    this->irradiance_photopic_sensor_->publish_state(this->calculated_values_.irradiance_photopic);
  }
  if (this->par_sensor_ != nullptr) {
    this->par_sensor_->publish_state(this->calculated_values_.par);
  }
  if (this->ppfd_sensor_ != nullptr) {
    this->ppfd_sensor_->publish_state(this->calculated_values_.ppfd);
  }
  if (this->color_temperature_sensor_ != nullptr) {
    this->color_temperature_sensor_->publish_state(this->calculated_values_.cct);
  }
  if (this->saturated_ != nullptr) {
    this->saturated_->publish_state(this->readings_.saturated);
  }
}

AS7343Gain AS7343Component::get_gain() {
  uint8_t data;
  this->read_byte((uint8_t) AS7343Registers::CFG1, &data);
  return (AS7343Gain) data;
}

uint8_t AS7343Component::get_atime() {
  uint8_t data;
  this->read_byte((uint8_t) AS7343Registers::ATIME, &data);
  return data;
}

uint16_t AS7343Component::get_astep() {
  uint16_t data;
  this->read_byte_16((uint8_t) AS7343Registers::ASTEP_LSB, &data);
  return this->swap_bytes(data);
}

float AS7343Component::get_gain_multiplier(AS7343Gain gain) {
  float gainx = ((uint16_t) 1 << (uint8_t) gain);
  // The AS7343 sensor's gain values are represented as powers of 2, but the actual gain multiplier
  // is half of this value. This division by 2 adjusts the calculated gain multiplier accordingly.
  return gainx / 2;
}

bool AS7343Component::setup_gain(AS7343Gain gain) {
  ESP_LOGD(TAG, "Setup gain %u", (uint8_t) gain);
  this->readings_.valid = false;
  return this->write_byte((uint8_t) AS7343Registers::CFG1, gain);
}

bool AS7343Component::setup_atime(uint8_t atime) {
  ESP_LOGD(TAG, "Setup atime %u", atime);
  this->readings_.valid = false;
  return this->write_byte((uint8_t) AS7343Registers::ATIME, atime);
}

bool AS7343Component::setup_astep(uint16_t astep) {
  ESP_LOGD(TAG, "Setup astep %u", astep);
  this->readings_.valid = false;
  return this->write_byte_16((uint8_t) AS7343Registers::ASTEP_LSB, swap_bytes(astep));
}

bool AS7343Component::change_gain(AS7343Gain gain) {
  this->gain_ = gain;
  this->enable_spectral_measurement(false);
  this->readings_.valid = false;
  return this->write_byte((uint8_t) AS7343Registers::CFG1, gain);
}
static float last_reported_gain = -1;

bool AS7343Component::check_if_adjustments_needed_() {
  this->auto_gain_tries_++;
  this->auto_gain_status_prev_ = this->auto_gain_status_;
  // check for saturation
  uint16_t max_adc = this->get_maximum_spectral_adc_(this->readings_.atime, this->readings_.astep);
  uint16_t min_adc = 0.5 * max_adc;
  uint16_t highest_adc = this->get_highest_value(this->readings_.raw_counts);

  if (highest_adc >= max_adc) {
    this->auto_gain_status_ = GAIN_TOO_HIGH;
    ESP_LOGW(TAG, "Raw Data Saturation: Highest reading: %u, max ADC: %u", highest_adc, max_adc);
  } else if (highest_adc < min_adc) {
    this->auto_gain_status_ = GAIN_TOO_LOW;
    ESP_LOGW(TAG, "Raw Data Underexposure: Highest reading: %u, max ADC: %u", highest_adc, max_adc);
  } else {
    this->auto_gain_tries_ = 0;
    this->auto_gain_status_ = GAIN_OK;
    // ESP_LOGD(TAG, "Good readings. Highest reading: %u, Max ADC: %u", highest_adc, max_adc);
  }

  //  ESP_LOGD(TAG, "  Gain  =  %.1f", this->readings_.gain_x);
  //  ESP_LOGD(TAG, "  ATIME = %u ", this->readings_.atime);
  //  ESP_LOGD(TAG, "  ASTEP = %u ", this->readings_.astep);
  //  ESP_LOGD(TAG, "  TINT  = %.3f ms", this->readings_.t_int_us / 1000.0f);

  if (this->auto_gain_status_ == GAIN_OK) {
    // ESP_LOGD(TAG, "Gain OK. No adjustments needed.");
    return false;
  }

  if (this->auto_gain_tries_ > MAX_AUTO_GAIN_TRIES) {
    ESP_LOGW(TAG, "Max auto gain tries reached. Stopping auto gain.");
    this->auto_gain_tries_ = 0;
    this->auto_gain_status_ = GAIN_OK;
    return false;
  }

  bool can_we_do_something = true;

  if (this->auto_gain_status_ == GAIN_TOO_HIGH) {
    if (this->readings_.gain == AS7343Gain::AS7343_GAIN_0_5X) {
      ESP_LOGW(TAG, "Gain too high. Can't reduce gain. Stopping auto gain.");
      can_we_do_something = false;
    } else {
      if (this->auto_gain_status_prev_ == GAIN_TOO_LOW) {
        // for now I think it might be better to get back to the previous gain and stop auto gain to avoid oscillations
        ESP_LOGW(TAG, "Gain too high after too low. Reducing gain and stopping.");
        this->auto_gain_tries_ = MAX_AUTO_GAIN_TRIES;
      } else {
        ESP_LOGW(TAG, "Gain too high. Reducing gain.");
      }
      this->change_gain((AS7343Gain) ((uint8_t) this->readings_.gain - 1));
    }
  } else if (this->auto_gain_status_ == GAIN_TOO_LOW) {
    // avoid increasing gain back if we just decreased it
    if (this->auto_gain_status_prev_ == GAIN_TOO_HIGH) {
      ESP_LOGW(TAG, "We will keep this gain to avoid getting too high gain. Stopping auto gain.");
      can_we_do_something = false;
    } else {
      if (this->readings_.gain == AS7343Gain::AS7343_GAIN_2048X) {
        ESP_LOGW(TAG, "Gain too low. Can't increase gain. Stopping auto gain.");
        can_we_do_something = false;
      } else {
        ESP_LOGW(TAG, "Gain too low. Increasing gain.");
        this->change_gain((AS7343Gain) ((uint8_t) this->readings_.gain + 1));
      }
    }
  }

  if (!can_we_do_something) {
    this->auto_gain_tries_ = 0;
    this->auto_gain_status_ = GAIN_OK;
    return false;
  }
  this->enable_spectral_measurement(true);

  // // try to optimize gain
  // uint8_t gain = this->readings_.gain;
  // bool saturation = false;
  // this->get_optimized_gain_(max_adc, highest_adc, 0, 3, gain, saturation);
  // if (saturation) {
  //   ESP_LOGW(TAG, "Gain too low. Setting gain to %u", gain);
  //   this->change_gain((AS7343Gain) gain);
  // } else {
  //   ESP_LOGD(TAG, "Gain OK. Setting gain to %u", gain);
  //   this->change_gain((AS7343Gain) gain);
  // }
  return true;
}

void AS7343Component::calculate_() {
  this->calculate_basic_counts_();

  ESP_LOGD(TAG, "Parameters:");
  ESP_LOGD(TAG, "  Gain  =  %.1f", this->readings_.gain_x);
  ESP_LOGD(TAG, "  TINT  = %.3f ms", this->readings_.t_int_us / 1000.0f);
  ESP_LOGD(TAG, "  ATIME = %u ", this->readings_.atime);
  ESP_LOGD(TAG, "  ASTEP = %u ", this->readings_.astep);

  uint16_t max_adc = this->get_maximum_spectral_adc_(this->readings_.atime, this->readings_.astep);
  uint16_t min_adc = 0.5 * max_adc;
  uint16_t highest_adc = this->get_highest_value(this->readings_.raw_counts);
  ESP_LOGD(TAG, "  Max ADC: %u", max_adc);

  ESP_LOGD(TAG, "Readings:");
  ESP_LOGD(TAG, "  Highest ADC: %.2f%%", (highest_adc / (float) max_adc) * 100.0f);
  log_cn_s(TAG, "  Channel", CHANNEL_NAME);
  log_cn_d(TAG, "  Nm", CHANNEL_WAVE_NM);
  log_cn_d(TAG, "  Counts", this->readings_.raw_counts);
  log_cn_f(TAG, "  Basic", this->calculated_values_.basic_counts);

  this->calculate_spectral_(this->calculated_values_.lux, this->calculated_values_.par, this->calculated_values_.ppfd,
                            this->calculated_values_.irradiance, this->calculated_values_.irradiance_photopic);

  this->calculate_color_(this->calculated_values_.cct, this->calculated_values_.duv,
                         this->calculated_values_.lux_from_xyz);

  this->calculate_cri_();

  //
  // check for wrong readings due to underexposure or overexposure, replace with 0
  //
  this->calculated_values_.lux = std::max(this->calculated_values_.lux, 0.0f);
  this->calculated_values_.irradiance = std::max(this->calculated_values_.irradiance, 0.0f);
  this->calculated_values_.irradiance_photopic = std::max(this->calculated_values_.irradiance_photopic, 0.0f);
  this->calculated_values_.par = std::max(this->calculated_values_.par, 0.0f);
  this->calculated_values_.ppfd = std::max(this->calculated_values_.ppfd, 0.0f);
  this->calculated_values_.lux_from_xyz = std::max(this->calculated_values_.lux_from_xyz, 0.0f);
  this->calculated_values_.cct = std::max(this->calculated_values_.cct, 0.0f);
  // this->calculated_values_.duv = std::max(this->calculated_values_.duv, 0.0f);

  //
  // apply glass attenuation
  //
  this->calculated_values_.irradiance *= this->glass_attenuation_factor_;
  this->calculated_values_.irradiance_photopic *= this->glass_attenuation_factor_;
  this->calculated_values_.lux *= this->glass_attenuation_factor_;
  this->calculated_values_.lux_from_xyz *= this->glass_attenuation_factor_;
  this->calculated_values_.ppfd *= this->glass_attenuation_factor_;
  this->calculated_values_.par *= this->glass_attenuation_factor_;

  ESP_LOGD(TAG, "Calculated values (glass attenuation = %.2f):", this->glass_attenuation_factor_);
  ESP_LOGD(TAG, "  Illuminance         : %f lx", this->calculated_values_.lux);
  ESP_LOGD(TAG, "  Irradiance          : %f W/m²", this->calculated_values_.irradiance);
  ESP_LOGD(TAG, "  Irradiance(photopic): %f W/m²", this->calculated_values_.irradiance_photopic);
  ESP_LOGD(TAG, "  PAR                 : %f W/m²", this->calculated_values_.par);
  ESP_LOGD(TAG, "  PPFD                : %f µmol/s⋅m²", this->calculated_values_.ppfd);
  ESP_LOGD(TAG, "  Color temp(XYZ)     : %f K", this->calculated_values_.cct);
  //  ESP_LOGD(TAG, "  Duv(XYZ)            : %f", this->calculated_values_.duv);
  ESP_LOGD(TAG, "  Illuminance(XYZ)    : %f lx", this->calculated_values_.lux_from_xyz);
}

void AS7343Component::calculate_basic_counts_() {
  // gain_x  : numeric gain (0.5 … 2048.0)
  // t_int_us: integration-time in micro-seconds
  const float inv_exposure = 1.0f / (this->readings_.gain_x * this->readings_.t_int_us);  // 1/(gain·µs)
  // const float us_to_s = 1.0e6f;

  for (size_t i = 0; i < AS7343_NUM_CHANNELS; i++) {
    // 1. raw → basic-counts  (now counts · s⁻¹ @ ×1 gain)
    float basic_count = this->readings_.raw_counts[i] * inv_exposure;  // * us_to_s;
    // 2. gain-linearity correction (datasheet “GAIN_CORR” table)
    basic_count *= AS7343_GAIN_CORRECTION[(uint8_t) this->readings_.gain][i];

    // when to check for dark readings?

    this->calculated_values_.basic_counts[i] = basic_count;  // units: counts·s⁻¹
  }
}

void AS7343Component::calculate_spectral_(float &lux, float &par, float &ppfd, float &irradiance,
                                          float &irradiance_photopic) {
  lux = ppfd = par = irradiance = irradiance_photopic = 0.0f;

  for (uint8_t i = 0; i < AS7343_NUM_CHANNELS; i++) {
    lux += AS7343_LUX_PER_COUNT[i] * this->calculated_values_.basic_counts[i];
    ppfd += AS7343_PPFD_UMOL_PER_COUNT[i] * this->calculated_values_.basic_counts[i];
    par += AS7343_PAR_E_MW_PER_COUNT[i] * this->calculated_values_.basic_counts[i];
    irradiance += AS7343_IRRAD_MW_PER_COUNT[i] * this->calculated_values_.basic_counts[i];
  }

  // scale values for proper units in case basic counts are in counts·s⁻¹ instead of counts·µs⁻¹
  // lux /= 1e6f;                      // convert to lux
  // par /= 1e6f;                      // convert to W/m²
  // ppfd /= 1e3f;                     // convert to µmol/s·m²
  // irradiance /= 1e6f;               // convert to W/m²

  ppfd *= 1e3f;  // convert to µmol/s·m²

  irradiance_photopic = lux / 683;  // convert to W/m²
}

void AS7343Component::calculate_color_(float &cct, float &duv, float &lux) {
  {
    ESP_LOGD(TAG, "Tristimulus:");
    float XYZ[3] = {0, 0, 0};
    for (uint8_t i = 0; i < AS7343_NUM_CHANNELS; i++) {
      for (uint8_t j = 0; j < 3; j++) {
        XYZ[j] += AS7343_XYZ_PER_COUNT[j][i] * this->calculated_values_.basic_counts[i];
      }
    }

    // basic counts are 1e6 bigger than in example calculations in excel sheet, since TINT is in seconds and not in us
    // XYZ[0] /= 1e6f;
    // XYZ[1] /= 1e6f;
    // XYZ[2] /= 1e6f;

    float epsilon = 0.0001;
    float xyz_sum = XYZ[0] + XYZ[1] + XYZ[2];
    float x{0}, y{0}, z{0};
    if (fabs(xyz_sum) < epsilon) {
      cct = 0;
      duv = NAN;
      lux = 0;
    } else {
      duv = NAN;
      x = XYZ[0] / xyz_sum;
      y = XYZ[1] / xyz_sum;
      z = XYZ[2] / xyz_sum;

      float n = (x - 0.3320) / (0.1858 - y);

      cct = 437 * pow(n, 3) + 3601 * pow(n, 2) + 6861 * n + 5517;

      // Calc lx from Y CIE1931
      lux = XYZ[1] * this->corr_lx_y_cie1931_;
      // output x y z (small)
      ESP_LOGD(TAG, "  XYZ: %.2f, %.2f, %.2f", XYZ[0], XYZ[1], XYZ[2]);
      ESP_LOGD(TAG, "  x: %.2f, y: %.2f, z: %.2f", x, y, z);
      ESP_LOGD(TAG, "  CCT: %.2f, duv: %.2f", cct, duv);
    }
  }
  // {
  //   ESP_LOGD(TAG, "COLORS 2:");
  //   // option two
  //   float XYZ[3] = {0, 0, 0};
  //   for (uint8_t i = 0; i < AS7343_NUM_CHANNELS; i++) {
  //     for (uint8_t j = 0; j < 3; j++) {
  //       XYZ[j] += AS7343_XYZ_PER_COUNT_2[j][i] * this->calculated_values_.basic_counts[i];
  //     }
  //   }
  //   float epsilon = 0.0001;
  //   float xyz_sum = XYZ[0] + XYZ[1] + XYZ[2];
  //   float x{0}, y{0}, z{0};
  //   if (xyz_sum < epsilon && xyz_sum > -epsilon) {
  //     cct = 0;
  //     duv = 0;
  //     lux = 0;
  //   } else {
  //     duv = 0;
  //     x = XYZ[0] / xyz_sum;
  //     y = XYZ[1] / xyz_sum;
  //     z = XYZ[2] / xyz_sum;

  //     float n = (x - 0.3320) / (0.1858 - y);

  //     cct = 437 * pow(n, 3) + 3601 * pow(n, 2) + 6861 * n + 5517;

  //     // Calc lx from Y CIE1931
  //     lux = XYZ[1] * this->corr_lx_y_cie1931_ / 1e6f;
  //     // output x y z (small)
  //     ESP_LOGD(TAG, "XYZ: %.2f, %.2f, %.2f", XYZ[0], XYZ[1], XYZ[2]);
  //     ESP_LOGD(TAG, "Output x: %.2f, y: %.2f, z: %.2f", x, y, z);
  //     ESP_LOGD(TAG, "CCT: %.2f, duv: %.2f", cct, duv);
  //   }
  // }
}

void AS7343Component::calculate_cri_() {
  // reconstruct spd first
  // use AS7343_CORR_MATRIX_380_780_NM
  float spd[401] = {};
  for (size_t nm = 380; nm < 780; nm++) {
    float spd_value = 0.0f;
    // no VIS/CLEAR channel
    for (uint8_t i = 0; i < AS7343_NUM_CHANNELS - 1; i++) {
      spd_value += AS7343_CORR_MATRIX_380_780_NM[nm - 380][i] * this->calculated_values_.basic_counts[i];
    }
    spd[nm - 380] = std::max(spd_value, 0.0f);
  }

  // do calcs
  // todo
}

bool AS7343Component::read_all_channels() {
  std::array<uint16_t, AS7343_NUM_CHANNELS_MAX> data;

  AS7343RegStatus status{0};
  status.raw = this->reg((uint8_t) AS7343Registers::STATUS).get();
  ESP_LOGVV(TAG, "Status 0x%02x, sint %d, fint %d, aint %d, asat %d", status.raw, status.sint, status.fint, status.aint,
            status.asat);
  this->reg((uint8_t) AS7343Registers::STATUS) = status.raw;

  AS7343RegAStatus astatus{0};
  astatus.raw = this->reg((uint8_t) AS7343Registers::ASTATUS).get();
  ESP_LOGVV(TAG, "AStatus 0x%02x, again_status %d, asat_status %d", astatus.raw, astatus.again_status,
            astatus.asat_status);

  if (astatus.asat_status) {
    ESP_LOGVV(TAG, "AS7343 affected by analog or digital saturation. Readings are not reliable.");
  }

  auto ret = this->read_bytes_16((uint8_t) AS7343Registers::DATA_O, data.data(), AS7343_NUM_CHANNELS_MAX);

  for (uint8_t i = 0; i < AS7343_NUM_CHANNELS; i++) {
    this->readings_.raw_counts[i] = data[SMUX_CHANNEL_MAP[i]];
  }

  // combine two clear channels to one
  uint16_t clear = data[AS7343_CHANNEL_CLEAR_0] / 2 + data[AS7343_CHANNEL_CLEAR_1] / 2;
  this->readings_.raw_counts[AS7343_NUM_CHANNELS - 1] = clear;

  this->readings_.saturated = astatus.asat_status;  // latched data affected by saturation
  this->readings_.gain = astatus.again_status;      // gain applied to the latest spectral measurement

  this->readings_.gain_x = this->get_gain_multiplier(this->readings_.gain);
  this->readings_.atime = this->get_atime();
  this->readings_.astep = this->get_astep();
  this->readings_.t_int_us = (1.0f + this->readings_.atime) * (1.0f + this->readings_.astep) * 2.78f;

  static uint32_t invokes = 0;
  invokes++;
  ESP_LOGD(TAG, "Readings %u:", invokes);

  return ret;
}

bool AS7343Component::is_data_ready() {
  AS7343RegStatus2 status2{0};
  status2.raw = this->reg((uint8_t) AS7343Registers::STATUS2).get();
  this->reg((uint8_t) AS7343Registers::STATUS2) = status2.raw;
  if (status2.avalid) {
    ESP_LOGVV(TAG, "Status2 0x%02x, avalid %d, asat_digital %d, asat_analog %d", status2.raw, status2.avalid,
              status2.asat_digital, status2.asat_analog);
  }

  //  return this->read_register_bit((uint8_t) AS7343Registers::STATUS2, 6);
  return status2.avalid;
}

void AS7343Component::set_bank_for_reg_(AS7343Registers reg) {
  bool bank = (uint8_t) reg < 0x80;
  if (bank == this->bank_) {
    return;
  }
  this->write_register_bit((uint8_t) AS7343Registers::CFG0, bank, AS7343_CFG0_REG_BANK_BIT);
  this->bank_ = bank;
}

bool AS7343Component::enable_power(bool enable) {
  return this->write_register_bit((uint8_t) AS7343Registers::ENABLE, enable, AS7343_ENABLE_PON_BIT);
}

bool AS7343Component::enable_spectral_measurement(bool enable) {
  this->readings_.valid = false;
  return this->write_register_bit((uint8_t) AS7343Registers::ENABLE, enable, AS7343_ENABLE_SP_EN_BIT);
}

bool AS7343Component::read_register_bit(uint8_t address, uint8_t bit_position) {
  uint8_t data;
  this->read_byte(address, &data);
  bool bit = (data & (1 << bit_position)) > 0;
  return bit;
}

bool AS7343Component::write_register_bit(uint8_t address, bool value, uint8_t bit_position) {
  if (value) {
    return this->set_register_bit(address, bit_position);
  }

  return this->clear_register_bit(address, bit_position);
}

bool AS7343Component::set_register_bit(uint8_t address, uint8_t bit_position) {
  uint8_t data;
  this->read_byte(address, &data);
  data |= (1 << bit_position);
  return this->write_byte(address, data);
}

bool AS7343Component::clear_register_bit(uint8_t address, uint8_t bit_position) {
  uint8_t data;
  this->read_byte(address, &data);
  data &= ~(1 << bit_position);
  return this->write_byte(address, data);
}

uint16_t AS7343Component::swap_bytes(uint16_t data) { return (data >> 8) | (data << 8); }

void AS7343Component::setup_tint_(float TINT) {
  ESP_LOGD(TAG, "Setup tint %.2f", TINT);
  uint8_t ATIME = 0x00;
  uint16_t ASTEP = 0x0000;
  while (true) {
    ASTEP = ((TINT / (double) (ATIME + 1)) * 720.0 / 2.0);

    if (abs(((ATIME + 1) * (ASTEP + 1) * 2 / 720) - (uint16_t) TINT) <= 1) {
      break;
    } else {
      ATIME += 1;
    }
  }

  this->setup_atime(ATIME);
  this->setup_astep(ASTEP);

  // this->write_byte((uint8_t)AS7343_ATIME, ATIME);
  // this->write_byte((uint8_t)AS7343_ASTEP_LSB, (uint8_t) (ASTEP & 0xFF));
  // this->write_byte((uint8_t)AS7343_ASTEP_MSB, (uint8_t) (ASTEP >> 8));
}

float AS7343Component::get_tint_() {
  uint16_t ASTEP = this->get_astep();
  uint8_t ATIME = this->get_atime();

  double TINT = (ASTEP + 1) * (ATIME + 1) * (2.0 / 720.0);

  return TINT;
}

void AS7343Component::optimizer_(float max_TINT) {
  // uint8_t currentGain = 12;

  // uint16_t FSR = 65535;
  // float TINT = 182.0;
  // // AS7343_set_TINT(handle, TINT);
  // this->setup_tint_(TINT);

  // uint16_t max_count;
  // uint16_t min_count;

  // while (true) {
  //   max_count = 0;
  //   min_count = 0xffff;
  //   this->setup_gain((AS7343Gain) currentGain);

  //   uint16_t data[18];
  //   this->enable_spectral_measurement(true);
  //   this->read_18_channels(data);
  //   this->enable_spectral_measurement(false);

  //   for (uint8_t i = 0; i < 18; i++) {
  //     if (i == 5 || i == 11 || i == 17) {
  //       continue;
  //     }
  //     if (data[i] > max_count) {
  //       max_count = data[i];
  //     }
  //     if (data[i] < min_count) {
  //       min_count = data[i];
  //     }
  //   }

  //   if (max_count > 0xE665) {
  //     if (currentGain == 0) {
  //       // TODO: send optimizer failed due to saturation message
  //       break;
  //     }
  //     currentGain -= 1;
  //     continue;
  //   }

  //   else if (min_count == 0) {
  //     if (currentGain == 12) {
  //       // TODO: send optimizer failed due to saturation message
  //       break;
  //     }
  //     currentGain += 1;
  //     continue;
  //   }

  //   else {
  //     break;
  //   }
  // }

  // float counts_expected = (float) max_count;
  // float multiplier = 0.90;

  // while (true) {
  //   // set to loop once only, might change the algorithm in the future
  //   max_count = 0;
  //   float exp = (multiplier * (float) FSR - counts_expected);
  //   if (exp < 0) {
  //     break;
  //   }
  //   float temp_TINT = TINT + pow(2, log((multiplier * (float) FSR - counts_expected)) / log(2)) * (2.0 / 720.0);

  //   if (temp_TINT > max_TINT) {
  //     break;
  //   }

  //   this->setup_tint_(temp_TINT);

  //   std::array<uint16_t,AS7343_NUM_CHANNELS> data;
  //   this->enable_spectral_measurement(true);
  //   this->read_18_channels(data.data());
  //   this->enable_spectral_measurement(false);

  //   for (uint8_t i = 0; i < 18; i++) {
  //     if (i == 5 || i == 11 || i == 17) {
  //       continue;
  //     }
  //     if (data[i] > max_count) {
  //       max_count = data[i];
  //     }
  //   }

  //   if (max_count >= multiplier * 0xFFEE) {
  //     multiplier = multiplier - 0.05;
  //     continue;
  //   } else {
  //     TINT = temp_TINT;
  //   }
  //   break;
  // }
  // // this->set_gain(currentGain);
  // this->setup_gain((AS7343Gain) currentGain);
  // this->setup_tint_(TINT);
}

void AS7343Component::direct_config_3_chain_() {
  this->write_byte((uint8_t) AS7343Registers::CFG6, 0x0);
  this->write_byte((uint8_t) AS7343Registers::FD_CFG0, 0xa1);
  this->write_byte((uint8_t) AS7343Registers::CFG10, 0xf2);

  this->write_byte((uint8_t) AS7343Registers::CFG0, 0x10);
  this->write_byte((uint8_t) AS7343Registers::CFG1, 0x0c);
  this->write_byte((uint8_t) AS7343Registers::CFG8, 0xc8);
  this->write_byte((uint8_t) AS7343Registers::CFG20, 0x62);
  this->write_byte((uint8_t) AS7343Registers::AGC_GAIN_MAX, 0x99);
  this->write_byte((uint8_t) AS7343Registers::FD_TIME_1, 0x64);
  this->write_byte((uint8_t) AS7343Registers::FD_TIME_2, 0x21);

  this->write_byte((uint8_t) 0xe7, 0x00);
  this->write_byte((uint8_t) 0xe4, 0x46);

  this->write_byte((uint8_t) 0xe7, 0x04);
  this->write_byte((uint8_t) 0xe4, 0x46);

  this->write_byte((uint8_t) 0xe7, 0x65);
  this->write_byte((uint8_t) 0xe4, 0x46);

  this->write_byte((uint8_t) 0xe7, 0x02);
  this->write_byte((uint8_t) 0xe4, 0x46);

  this->write_byte((uint8_t) 0xe7, 0x00);
  this->write_byte((uint8_t) 0xe4, 0x46);

  this->write_byte((uint8_t) 0xe7, 0x05);
  this->write_byte((uint8_t) 0xe4, 0x46);

  this->write_byte((uint8_t) 0xe7, 0x00);
  this->write_byte((uint8_t) 0xe4, 0x46);

  this->write_byte((uint8_t) 0xe7, 0x01);
  this->write_byte((uint8_t) 0xe4, 0x46);

  this->write_byte((uint8_t) 0xe7, 0x00);
  this->write_byte((uint8_t) 0xe4, 0x46);

  this->write_byte((uint8_t) 0xe7, 0x30);
  this->write_byte((uint8_t) 0xe4, 0x46);

  this->write_byte((uint8_t) 0xe7, 0x00);
  this->write_byte((uint8_t) 0xe4, 0x56);

  this->write_byte((uint8_t) 0xe7, 0x00);
  this->write_byte((uint8_t) 0xe4, 0x56);

  this->write_byte((uint8_t) 0xe7, 0x60);
  this->write_byte((uint8_t) 0xe4, 0x56);

  this->write_byte((uint8_t) 0xe7, 0x20);
  this->write_byte((uint8_t) 0xe4, 0x56);

  this->write_byte((uint8_t) 0xe7, 0x04);
  this->write_byte((uint8_t) 0xe4, 0x56);

  this->write_byte((uint8_t) 0xe7, 0x50);
  this->write_byte((uint8_t) 0xe4, 0x56);

  this->write_byte((uint8_t) 0xe7, 0x03);
  this->write_byte((uint8_t) 0xe4, 0x56);

  this->write_byte((uint8_t) 0xe7, 0x00);
  this->write_byte((uint8_t) 0xe4, 0x56);

  this->write_byte((uint8_t) 0xe7, 0x01);
  this->write_byte((uint8_t) 0xe4, 0x56);

  this->write_byte((uint8_t) 0xe7, 0x05);
  this->write_byte((uint8_t) 0xe4, 0x56);

  this->write_byte((uint8_t) 0xe7, 0x05);
  this->write_byte((uint8_t) 0xe4, 0x66);

  this->write_byte((uint8_t) 0xe7, 0x00);
  this->write_byte((uint8_t) 0xe4, 0x66);

  this->write_byte((uint8_t) 0xe7, 0x60);
  this->write_byte((uint8_t) 0xe4, 0x66);

  this->write_byte((uint8_t) 0xe7, 0x00);
  this->write_byte((uint8_t) 0xe4, 0x66);

  this->write_byte((uint8_t) 0xe7, 0x30);
  this->write_byte((uint8_t) 0xe4, 0x66);

  this->write_byte((uint8_t) 0xe7, 0x00);
  this->write_byte((uint8_t) 0xe4, 0x66);

  this->write_byte((uint8_t) 0xe7, 0x40);
  this->write_byte((uint8_t) 0xe4, 0x66);

  this->write_byte((uint8_t) 0xe7, 0x10);
  this->write_byte((uint8_t) 0xe4, 0x66);

  this->write_byte((uint8_t) 0xe7, 0x20);
  this->write_byte((uint8_t) 0xe4, 0x66);

  this->write_byte((uint8_t) 0xe7, 0x00);
  this->write_byte((uint8_t) 0xe4, 0x66);

  // this->write_byte((uint8_t)0x80, 0x11);
}

const uint16_t MIN_ASTEP = 1;
const uint16_t MAX_ASTEP = 65534;
const uint32_t MIN_ITIME_US = 6;
const uint32_t MAX_ITIME_US = 46602667;
const uint32_t CONVERSION_FACTOR_MS_TO_US = 1000;

const uint32_t INTEGRATION_TIME_STEP_US_FACTOR = 2000;
const uint32_t INTEGRATION_TIME_STEP_US_DIVIDER = 720;

/*! Use this macro for signed 64 Bit divisions */
#define DIV64_S64(s64dividend, s64divisor) (s64dividend / s64divisor)
/*! Use this macro for unsigned 64 Bit divisions */
#define DIV64_U64(u64dividend, u64divisor) (u64dividend / u64divisor)

bool AS7343Component::as7352_set_integration_time_us(uint32_t time_us) {
  bool result;
  int64_t time;
  uint8_t atime = 0;
  uint16_t astep = 0xFFFF / 2;
  int64_t astep_i64;

  if (MIN_ITIME_US > time_us || MAX_ITIME_US < time_us) {
    return false;
  }

  time = DIV64_S64((int64_t) time_us * INTEGRATION_TIME_STEP_US_DIVIDER, INTEGRATION_TIME_STEP_US_FACTOR);
  time = DIV64_S64(time, ((int64_t) astep + 1));
  time -= 1;

  if (0 > time) {
    atime = 0;
  } else if (255 < time) {
    atime = 255;
  } else {
    atime = (uint8_t) time;
  }

  astep_i64 = DIV64_S64((int64_t) time_us * INTEGRATION_TIME_STEP_US_DIVIDER * 10, INTEGRATION_TIME_STEP_US_FACTOR);
  astep_i64 = DIV64_S64(astep_i64, ((int64_t) atime + 1)) + 5;
  astep_i64 = DIV64_S64(astep_i64, 10);
  astep_i64 -= 1;

  if (MIN_ASTEP > astep_i64 || MAX_ASTEP < astep_i64) {
    return false;
  } else {
    astep = (uint16_t) astep_i64;
  }

  auto max_adc = this->get_maximum_spectral_adc_(atime, astep);
  ESP_LOGD(TAG, "for itime %u : atime %u, astep %u, max_adc: %u", time_us, atime, astep, max_adc);

  this->set_astep(astep);
  this->set_atime(atime);

  this->setup_atime(atime);
  this->setup_astep(astep);

  return result;
}

uint16_t AS7343Component::get_maximum_spectral_adc_() {
  //
  return this->get_maximum_spectral_adc_(this->atime_, this->astep_);
};

static constexpr uint16_t MAX_ADC_COUNT = 65535;

uint16_t AS7343Component::get_maximum_spectral_adc_(uint16_t atime, uint16_t astep) {
  uint32_t value = (atime + 1) * (astep + 1);
  if (value > MAX_ADC_COUNT) {
    value = MAX_ADC_COUNT;
  }
  return value;
}

// uint16_t AS7343Component::get_highest_value(std::array<uint16_t, AS7343_NUM_CHANNELS> &data) {
template<typename T, size_t N> T AS7343Component::get_highest_value(std::array<T, N> &data) {
  T max = 0;
  for (const auto &v : data) {
    if (v > max) {
      max = v;
    }
  }
  return max;
}

uint8_t LOW_AUTO_GAIN_VALUE = 3;
uint8_t AUTO_GAIN_DIVIDER = 2;
//#define IS_SATURATION 1
static constexpr uint32_t SATURATION_LOW_PERCENT = 80;
static constexpr uint32_t SATURATION_HIGH_PERCENT = 100;
static constexpr uint32_t ADC_SATURATED_VALUE = 65535;

bool AS7343Component::spectral_post_process_(bool fire_at_will) {
  bool need_to_repeat = false;
  // uint16_t highest_value, maximum_adc;
  // bool is_saturation{false};
  // uint8_t current_gain, new_gain;

  // uint16_t max_adc = this->get_maximum_spectral_adc_();
  // uint16_t highest_adc = this->get_highest_value(this->channel_readings_);

  // current_gain = this->readings_gain_;
  // new_gain = current_gain;
  // this->get_optimized_gain_(max_adc, highest_adc, AS7343Gain::AS7343_GAIN_0_5X, AS7343Gain::AS7343_GAIN_128X,
  // new_gain,
  //                           is_saturation);
  // if (new_gain != current_gain) {
  //   if (fire_at_will) {
  //     // need to repeat the measurement
  //     this->set_gain((AS7343Gain) new_gain);
  //     this->setup_gain((AS7343Gain) new_gain);
  //   }
  //   need_to_repeat = true;
  // } else if (is_saturation) {
  //   // digital saturation
  //   // but can't change gain? try change time ?
  //   ESP_LOGW(TAG, "Spectral post process: OPTIMIZE saturation detected");
  // }
  // if (!is_saturation) {
  //   // no saturation
  //   for (uint8_t i = 0; i < AS7343_NUM_CHANNELS; i++) {
  //     // todo - update reading with gain factor first, then compare
  //     if (this->channel_readings_[i] >= max_adc) {  // check both values - before and after gain factor application
  //       this->channel_readings_[i] = ADC_SATURATED_VALUE;
  //       is_saturation = true;
  //     }
  //   }
  //   if (is_saturation) {
  //     ESP_LOGW(TAG, "Spectral post process: CHANNEL saturation detected");
  //   }
  // }
  /// what to do with saturation and !need_to_repeat ?
  // ESP_LOGW(TAG, "Spectral post process: gain %u, saturation %u, need to repeat %u", new_gain, is_saturation,
  //          need_to_repeat);
  return need_to_repeat;
}

static uint8_t find_highest_bit(uint32_t value) {
  uint8_t i = 0;
  uint8_t order = 0;

  for (i = 0; i < 32; i++) {
    if (value == 0) {
      break;
    } else if ((value >> i) & 1) {
      order = i;
    }
  }

  return order;
}

void AS7343Component::get_optimized_gain_(uint16_t maximum_adc, uint16_t highest_adc, uint8_t lower_gain_limit,
                                          uint8_t upper_gain_limit, uint8_t &out_gain, bool &out_saturation) {
  uint32_t gain_change;

  if (highest_adc == 0) {
    highest_adc = 1;
  }

  if (highest_adc >= maximum_adc) {
    /* saturation detected */
    if (out_gain > LOW_AUTO_GAIN_VALUE) {
      out_gain /= AUTO_GAIN_DIVIDER;
    } else {
      out_gain = lower_gain_limit;
    }
    out_saturation = true;
  } else {
    /* value too low, increase the gain */
    gain_change =
        (SATURATION_LOW_PERCENT * (uint32_t) maximum_adc) / (SATURATION_HIGH_PERCENT * (uint32_t) highest_adc);
    if (gain_change == 0 && out_gain != 0) {
      (out_gain)--;
    } else {
      gain_change = find_highest_bit(gain_change);
      if (((uint32_t) (out_gain) + gain_change) > upper_gain_limit) {
        out_gain = upper_gain_limit;
      } else {
        out_gain += (uint8_t) gain_change;
      }
    }
    out_saturation = false;
  }

  if (lower_gain_limit > out_gain) {
    out_gain = lower_gain_limit;
  }
}

}  // namespace as7343
}  // namespace esphome
