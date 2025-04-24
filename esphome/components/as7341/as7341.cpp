#include "as7341.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace as7341 {

static const char *const TAG = "as7341";

static constexpr uint32_t DATA_COLLECTION_TIMEOUT_MS = 30 * 1000;  // 30 seconds

static constexpr std::array<const char *, AS7341_NUM_CHANNELS> CHANNEL_NAME = {
    "F1", "F2", "F3", "F4", "x", "xx", "F5", "F6", "F7", "F8", "Clear", "NIR",
};

static constexpr std::array<uint16_t, AS7341_NUM_CHANNELS> CHANNEL_WAVE_NM = {
    410, 440, 470, 510, 0, 0, 590, 630, 680, 730, 850, 940,
};

static constexpr std::array<float, AS7341_GAIN_COUNT> AS7341_GAIN_CORRECTION = {1.06, 1.05, 1.05, 1.05, 1.02, 1.02,
                                                                                1.01, 1.00, 1.00, 0.99, 0.96};

// LOGGING. printing out channel values except for last one which is fake just for the safety of inverse mapping
void log_cn_f(const char *TAG, const char *str, const std::array<float, AS7341_NUM_CHANNELS> &arr) {
  ESP_LOGD(TAG, "%8s, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, ", str, arr[0], arr[1], arr[2],
           arr[3], arr[6], arr[7], arr[8], arr[9], arr[10], arr[11]);
}

// LOGGING. printing out channel values except for last one which is fake just for the safety of inverse mapping
void log_cn_f0(const char *TAG, const char *str, const std::array<float, AS7341_NUM_CHANNELS> &arr) {
  ESP_LOGD(TAG, "%s, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, ", str, arr[0], arr[1], arr[2], arr[3],
           arr[6], arr[7], arr[8], arr[9], arr[10], arr[11]);
}

// LOGGING. printing out channel values except for last one which is fake just for the safety of inverse mapping
void log_cn_d(const char *TAG, const char *str, const std::array<uint16_t, AS7341_NUM_CHANNELS> &arr) {
  ESP_LOGD(TAG, "%s, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, ", str, arr[0], arr[1], arr[2], arr[3], arr[6], arr[7],
           arr[8], arr[9], arr[10], arr[11]);
}

// LOGGING. printing out channel values except for last one which is fake just for the safety of inverse mapping
void log_cn_s(const char *TAG, const char *str, const std::array<const char *, AS7341_NUM_CHANNELS> &arr) {
  ESP_LOGD(TAG, "%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, ", str, arr[0], arr[1], arr[2], arr[3], arr[6], arr[7],
           arr[8], arr[9], arr[10], arr[11]);
}

static const float AS7341_PAR_E_MW_PER_COUNT[AS7341_NUM_CHANNELS] = {
    5.936230324, 3.572528495, 2.914113437, 2.452851227, 0,         0,
    1.940656433, 1.533976606, 1.81983825,  1.232942274, -1.102368, -0.033304813};
static const float AS7341_PPFD_UMOL_PER_COUNT[AS7341_NUM_CHANNELS] = {
    0.021326768,  -0.000755828, 0.002035903,  0.000339802,  0,           0,
    -0.000200202, -0.000274651, -0.000413752, -0.000562557, 0.018267892, 0.006056582};

static const float AS7341_LUX_PER_COUNT[AS7341_NUM_CHANNELS] = {
    3.593463813, 165.7062172, 119.171583,   1010.497868,  0,           0, 1302.357073,
    793.6857108, 315.8020193, -12.15369248, -178.0792052, -15.43858344};

static const float AS7341_IRRAD_MW_PER_COUNT[AS7341_NUM_CHANNELS] = {8.110691277, 0.988166817, 1.513167954, 0.823071218,
                                                                     0,           0,           0.529502926, 0.35304952,
                                                                     0.255581795, 0.132104073, 2.423065737, 0.63983804};

const float AS7341_XYZ_PER_COUNT[3][AS7341_NUM_CHANNELS] = {
    {0.39814, 1.29540, 0.36956, 0.10902, 0, 0, 0.71942, 1.78180, 1.10110, -0.03991, -0.27597, -0.02347},
    {0.01396, 0.16748, 0.23538, 1.42750, 0, 0, 1.88670, 1.14200, 0.46497, -0.02702, -0.24468, -0.01993},
    {1.95010, 6.45490, 2.78010, 0.18501, 0, 0, 0.15325, 0.09539, 0.10563, 0.08866, -0.61140, -0.00938}

};

// const float AS7341_XYZ_PER_COUNT_2[3][AS7341_NUM_CHANNELS] = {
//     {0.370805966, 1.403295426, 0.334063159, 0.148146996, 0, 0, 0.686489902, 1.8604383, 1.07625649, -0.031292697,
//      -0.28972067, -0.028251949},
//     {0.005261294, 0.242615252, 0.174482552, 1.479499075, 0, 0, 1.906818555, 1.162058142, 0.462374845, -0.017794572,
//      -0.260730901, -0.022604075},
//     {1.812270271, 6.80862392, 2.759977373, 0.155029035, 0, 0, 0.197316295, 0.110543682, 0.103059235, 0.124949717,
//      -0.647614895, -0.013296015}};

void AS7341Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up AS7341...");
  LOG_I2C_DEVICE(this);

  // Verify device ID
  uint8_t id;
  this->read_byte(AS7341_ID, &id);
  ESP_LOGCONFIG(TAG, "  Read ID: 0x%X", id);
  if ((id & 0xFC) != (AS7341_CHIP_ID << 2)) {
    this->mark_failed();
    return;
  }

  // Power on (enter IDLE state)
  if (!this->enable_power(true)) {
    ESP_LOGE(TAG, "  Power on failed!");
    this->mark_failed();
    return;
  }

  // Set configuration
  this->write_byte(AS7341_CONFIG, 0x00);
  this->setup_atime(this->atime_);
  this->setup_astep(this->astep_);
  this->setup_gain(this->gain_);

  this->enable_spectral_measurement(true);

  this->state_ = State::IDLE;
}

void AS7341Component::dump_config() {
  ESP_LOGCONFIG(TAG, "AS7341:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with AS7341 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Gain: %u", get_gain());
  ESP_LOGCONFIG(TAG, "  ATIME: %u", get_atime());
  ESP_LOGCONFIG(TAG, "  ASTEP: %u", get_astep());

  LOG_SENSOR("  ", "F1", this->f1_);
  LOG_SENSOR("  ", "F2", this->f2_);
  LOG_SENSOR("  ", "F3", this->f3_);
  LOG_SENSOR("  ", "F4", this->f4_);
  LOG_SENSOR("  ", "F5", this->f5_);
  LOG_SENSOR("  ", "F6", this->f6_);
  LOG_SENSOR("  ", "F7", this->f7_);
  LOG_SENSOR("  ", "F8", this->f8_);
  LOG_SENSOR("  ", "Clear", this->clear_);
  LOG_SENSOR("  ", "NIR", this->nir_);
}

float AS7341Component::get_setup_priority() const { return setup_priority::DATA; }

void AS7341Component::update() {
  if (this->is_ready() && this->state_ == State::IDLE) {
    ESP_LOGV(TAG, "Initiating new data collection");
    this->state_ = State::START_MEASUREMENT;
  } else {
    ESP_LOGV(TAG, "Can't initiate new data collection - component not ready");
  }
}

void AS7341Component::loop() {
  if (this->is_ready()) {
    switch (this->state_) {
      case State::IDLE:
        // doing nothing, having best time
        break;

      case State::START_MEASUREMENT:
        // start measurement
        this->readings_.millis_start = millis();
        this->enable_spectral_measurement(true);
        this->state_ = State::COLLECTING_DATA;
        break;

      case State::COLLECTING_DATA:
        if (this->is_data_ready()) {
          this->read_channels();
          this->enable_spectral_measurement(false);
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
        // apply modifications
        this->calculate_();
        this->state_ = State::READY_TO_PUBLISH_PART_1;
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
void AS7341Component::publish_channel_readings_() {
  if (this->f1_ != nullptr)
    this->f1_->publish_state(this->channel_readings_[0]);
  if (this->f2_ != nullptr)
    this->f2_->publish_state(this->channel_readings_[1]);
  if (this->f3_ != nullptr)
    this->f3_->publish_state(this->channel_readings_[2]);
  if (this->f4_ != nullptr)
    this->f4_->publish_state(this->channel_readings_[3]);
  if (this->f5_ != nullptr)
    this->f5_->publish_state(this->channel_readings_[6]);
  if (this->f6_ != nullptr)
    this->f6_->publish_state(this->channel_readings_[7]);
  if (this->f7_ != nullptr)
    this->f7_->publish_state(this->channel_readings_[8]);
  if (this->f8_ != nullptr)
    this->f8_->publish_state(this->channel_readings_[9]);
  if (this->clear_ != nullptr)
    this->clear_->publish_state(this->channel_readings_[10]);
  if (this->nir_ != nullptr)
    this->nir_->publish_state(this->channel_readings_[11]);
}

void AS7341Component::publish_derived_readings_() {}

void AS7341Component::calculate_() {
  this->readings_.gain = this->get_gain();
  this->readings_.gain_x = this->get_gain_multiplier(this->readings_.gain);
  this->readings_.atime = this->get_atime();
  this->readings_.astep = this->get_astep();
  this->readings_.t_int_us = (1.0f + this->readings_.atime) * (1.0f + this->readings_.astep) * 2.78f;

  this->calculate_basic_counts_();

  ESP_LOGD(TAG, "Readings as7341:");
  log_cn_s(TAG, "  Channel", CHANNEL_NAME);
  log_cn_d(TAG, "  Nm", CHANNEL_WAVE_NM);
  log_cn_d(TAG, "  Counts", this->channel_readings_);
  log_cn_f(TAG, "  Basic", this->readings_.basic_counts);

  ESP_LOGD(TAG, "Parameters:");
  ESP_LOGD(TAG, "  Gain  =  %.1f", this->readings_.gain_x);
  ESP_LOGD(TAG, "  ATIME = %u ", this->readings_.atime);
  ESP_LOGD(TAG, "  ASTEP = %u ", this->readings_.astep);
  ESP_LOGD(TAG, "  TINT  = %.4f ms", this->readings_.t_int_us / 1000.0f);

  this->calculate_spectral_(this->calculated_values_.lux, this->calculated_values_.par, this->calculated_values_.ppfd,
                            this->calculated_values_.irradiance, this->calculated_values_.irradiance_photopic);
  this->calculate_color_(this->calculated_values_.cct, this->calculated_values_.duv,
                         this->calculated_values_.lux_from_xyz);

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
  this->calculated_values_.duv = std::max(this->calculated_values_.duv, 0.0f);

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

void AS7341Component::calculate_spectral_(float &lux, float &par, float &ppfd, float &irradiance,
                                          float &irradiance_photopic) {
  for (uint8_t i = 0; i < AS7341_NUM_CHANNELS; i++) {
    lux += AS7341_LUX_PER_COUNT[i] * this->readings_.basic_counts[i];
    ppfd += AS7341_PPFD_UMOL_PER_COUNT[i] * this->readings_.basic_counts[i];
    par += AS7341_PAR_E_MW_PER_COUNT[i] * this->readings_.basic_counts[i];
    irradiance += AS7341_IRRAD_MW_PER_COUNT[i] * this->readings_.basic_counts[i];
  }

  // scale values to proper units
  lux /= 1e6f;                      // convert to lux
  par /= 1e6f;                      // convert to W/m²
  ppfd /= 1e3f;                     // convert to µmol/s·m²
  irradiance /= 1e6f;               // convert to W/m²
  irradiance_photopic = lux / 683;  // convert to W/m²
}

void AS7341Component::calculate_color_(float &cct, float &duv, float &lux) {
  float XYZ[3] = {0, 0, 0};
  for (uint8_t i = 0; i < AS7341_NUM_CHANNELS; i++) {
    for (uint8_t j = 0; j < 3; j++) {
      XYZ[j] += AS7341_XYZ_PER_COUNT[j][i] * this->readings_.basic_counts[i];
    }
  }

  XYZ[0] /= 1e6f;
  XYZ[1] /= 1e6f;
  XYZ[2] /= 1e6f;

  float epsilon = 0.0001;
  float xyz_sum = XYZ[0] + XYZ[1] + XYZ[2];
  float x{0}, y{0}, z{0};
  if (xyz_sum < epsilon && xyz_sum > -epsilon) {
    cct = 0;
    duv = 0;
    lux = 0;
  } else {
    duv = 0;
    x = XYZ[0] / xyz_sum;
    y = XYZ[1] / xyz_sum;
    z = XYZ[2] / xyz_sum;

    float n = (x - 0.3320) / (0.1858 - y);

    cct = 437 * pow(n, 3) + 3601 * pow(n, 2) + 6861 * n + 5517;

    // Calc lx from Y CIE1931
    lux = XYZ[1] * 683;
    //      lux = XYZ[1] * this->corr_lx_y_cie1931_ / 1e6f;
    // output x y z (small)
    ESP_LOGD(TAG, "XYZ: %.2f, %.2f, %.2f", XYZ[0], XYZ[1], XYZ[2]);
    ESP_LOGD(TAG, "Output x: %.2f, y: %.2f, z: %.2f", x, y, z);
    ESP_LOGD(TAG, "CCT: %.2f, duv: %.2f", cct, duv);
  }
}

float AS7341Component::get_gain_multiplier(AS7341Gain gain) {
  float gainx = ((uint16_t) 1 << (uint8_t) gain);
  // The AS7341 sensor's gain values are represented as powers of 2, but the actual gain multiplier
  // is half of this value. This division by 2 adjusts the calculated gain multiplier accordingly.
}

void AS7341Component::calculate_basic_counts_() {
  this->readings_.basic_counts.fill(0.0f);
  // gain_x  : numeric gain (0.5 … 512.0)
  // t_int_us: integration-time in micro-seconds
  const float inv_exposure = 1.0f / (this->readings_.gain_x * this->readings_.t_int_us);  // 1/(gain·µs)
  const float us_to_s = 1.0e6f;

  for (size_t i = 0; i < AS7341_NUM_CHANNELS; i++) {
    // 1. raw → basic-counts  (now counts · s⁻¹ @ ×1 gain)
    float basic_count = this->channel_readings_[i] * inv_exposure * us_to_s;
    // // 2. gain-linearity correction (datasheet “GAIN_CORR” table)
    basic_count *= AS7341_GAIN_CORRECTION[(uint8_t) this->readings_.gain];

    // when to check for dark readings?

    this->readings_.basic_counts[i] = basic_count;  // units: counts·s⁻¹
  }
}

AS7341Gain AS7341Component::get_gain() {
  uint8_t data;
  this->read_byte(AS7341_CFG1, &data);
  return (AS7341Gain) data;
}

uint8_t AS7341Component::get_atime() {
  uint8_t data;
  this->read_byte(AS7341_ATIME, &data);
  return data;
}

uint16_t AS7341Component::get_astep() {
  uint16_t data;
  this->read_byte_16(AS7341_ASTEP, &data);
  return this->swap_bytes(data);
}

bool AS7341Component::setup_gain(AS7341Gain gain) { return this->write_byte(AS7341_CFG1, gain); }

bool AS7341Component::setup_atime(uint8_t atime) { return this->write_byte(AS7341_ATIME, atime); }

bool AS7341Component::setup_astep(uint16_t astep) { return this->write_byte_16(AS7341_ASTEP, swap_bytes(astep)); }

bool AS7341Component::read_channels() {
  auto data = this->channel_readings_.data();
  this->set_smux_low_channels(true);
  this->enable_spectral_measurement(true);
  this->wait_for_data();
  bool low_success = this->read_bytes_16(AS7341_CH0_DATA_L, data, 6);

  this->set_smux_low_channels(false);
  this->enable_spectral_measurement(true);
  this->wait_for_data();
  bool high_sucess = this->read_bytes_16(AS7341_CH0_DATA_L, &data[6], 6);

  return low_success && high_sucess;
}

void AS7341Component::set_smux_low_channels(bool enable) {
  this->enable_spectral_measurement(false);
  this->set_smux_command(AS7341_SMUX_CMD_WRITE);

  if (enable) {
    this->configure_smux_low_channels();

  } else {
    this->configure_smux_high_channels();
  }
  this->enable_smux();
}

bool AS7341Component::set_smux_command(AS7341SmuxCommand command) {
  uint8_t data = command << 3;  // Write to bits 4:3 of the register
  return this->write_byte(AS7341_CFG6, data);
}

void AS7341Component::configure_smux_low_channels() {
  // SMUX Config for F1,F2,F3,F4,NIR,Clear
  this->write_byte(0x00, 0x30);  // F3 left set to ADC2
  this->write_byte(0x01, 0x01);  // F1 left set to ADC0
  this->write_byte(0x02, 0x00);  // Reserved or disabled
  this->write_byte(0x03, 0x00);  // F8 left disabled
  this->write_byte(0x04, 0x00);  // F6 left disabled
  this->write_byte(0x05, 0x42);  // F4 left connected to ADC3/f2 left connected to ADC1
  this->write_byte(0x06, 0x00);  // F5 left disbled
  this->write_byte(0x07, 0x00);  // F7 left disbled
  this->write_byte(0x08, 0x50);  // CLEAR connected to ADC4
  this->write_byte(0x09, 0x00);  // F5 right disabled
  this->write_byte(0x0A, 0x00);  // F7 right disabled
  this->write_byte(0x0B, 0x00);  // Reserved or disabled
  this->write_byte(0x0C, 0x20);  // F2 right connected to ADC1
  this->write_byte(0x0D, 0x04);  // F4 right connected to ADC3
  this->write_byte(0x0E, 0x00);  // F6/F8 right disabled
  this->write_byte(0x0F, 0x30);  // F3 right connected to AD2
  this->write_byte(0x10, 0x01);  // F1 right connected to AD0
  this->write_byte(0x11, 0x50);  // CLEAR right connected to AD4
  this->write_byte(0x12, 0x00);  // Reserved or disabled
  this->write_byte(0x13, 0x06);  // NIR connected to ADC5
}

void AS7341Component::configure_smux_high_channels() {
  // SMUX Config for F5,F6,F7,F8,NIR,Clear
  this->write_byte(0x00, 0x00);  // F3 left disable
  this->write_byte(0x01, 0x00);  // F1 left disable
  this->write_byte(0x02, 0x00);  // reserved/disable
  this->write_byte(0x03, 0x40);  // F8 left connected to ADC3
  this->write_byte(0x04, 0x02);  // F6 left connected to ADC1
  this->write_byte(0x05, 0x00);  // F4/ F2 disabled
  this->write_byte(0x06, 0x10);  // F5 left connected to ADC0
  this->write_byte(0x07, 0x03);  // F7 left connected to ADC2
  this->write_byte(0x08, 0x50);  // CLEAR Connected to ADC4
  this->write_byte(0x09, 0x10);  // F5 right connected to ADC0
  this->write_byte(0x0A, 0x03);  // F7 right connected to ADC2
  this->write_byte(0x0B, 0x00);  // Reserved or disabled
  this->write_byte(0x0C, 0x00);  // F2 right disabled
  this->write_byte(0x0D, 0x00);  // F4 right disabled
  this->write_byte(0x0E, 0x24);  // F8 right connected to ADC2/ F6 right connected to ADC1
  this->write_byte(0x0F, 0x00);  // F3 right disabled
  this->write_byte(0x10, 0x00);  // F1 right disabled
  this->write_byte(0x11, 0x50);  // CLEAR right connected to AD4
  this->write_byte(0x12, 0x00);  // Reserved or disabled
  this->write_byte(0x13, 0x06);  // NIR connected to ADC5
}

bool AS7341Component::enable_smux() {
  this->set_register_bit(AS7341_ENABLE, 4);

  uint16_t timeout = 1000;
  for (uint16_t time = 0; time < timeout; time++) {
    // The SMUXEN bit is cleared once the SMUX operation is finished
    bool smuxen = this->read_register_bit(AS7341_ENABLE, 4);
    if (!smuxen) {
      return true;
    }

    delay(1);
  }

  return false;
}

bool AS7341Component::wait_for_data() {
  uint16_t timeout = 1000;
  for (uint16_t time = 0; time < timeout; time++) {
    if (this->is_data_ready()) {
      return true;
    }

    delay(1);
  }

  return false;
}

bool AS7341Component::is_data_ready() { return this->read_register_bit(AS7341_STATUS2, 6); }

bool AS7341Component::enable_power(bool enable) { return this->write_register_bit(AS7341_ENABLE, enable, 0); }

bool AS7341Component::enable_spectral_measurement(bool enable) {
  return this->write_register_bit(AS7341_ENABLE, enable, 1);
}

bool AS7341Component::read_register_bit(uint8_t address, uint8_t bit_position) {
  uint8_t data;
  this->read_byte(address, &data);
  bool bit = (data & (1 << bit_position)) > 0;
  return bit;
}

bool AS7341Component::write_register_bit(uint8_t address, bool value, uint8_t bit_position) {
  if (value) {
    return this->set_register_bit(address, bit_position);
  }

  return this->clear_register_bit(address, bit_position);
}

bool AS7341Component::set_register_bit(uint8_t address, uint8_t bit_position) {
  uint8_t data;
  this->read_byte(address, &data);
  data |= (1 << bit_position);
  return this->write_byte(address, data);
}

bool AS7341Component::clear_register_bit(uint8_t address, uint8_t bit_position) {
  uint8_t data;
  this->read_byte(address, &data);
  data &= ~(1 << bit_position);
  return this->write_byte(address, data);
}

uint16_t AS7341Component::swap_bytes(uint16_t data) { return (data >> 8) | (data << 8); }

}  // namespace as7341
}  // namespace esphome
