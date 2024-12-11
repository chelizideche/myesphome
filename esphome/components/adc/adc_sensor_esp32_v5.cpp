#ifdef USE_ESP32

#include "adc_sensor.h"
#include "esphome/core/log.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))

namespace esphome {
namespace adc {

static const char *const TAG = "adc.esp32.v5";

void ADCSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADC '%s'...", this->get_name().c_str());

  if (!this->do_setup_) {
    return;
  }

  if (this->is_adc1_) {
    if (this->adc1_handle_ == nullptr) {
      adc_oneshot_unit_init_cfg_t init_config1 = {};  // Zero initialize
      init_config1.unit_id = ADC_UNIT_1;
      init_config1.ulp_mode = ADC_ULP_MODE_DISABLE;
#if USE_ESP32_VARIANT_ESP32C3 || USE_ESP32_VARIANT_ESP32C6 || USE_ESP32_VARIANT_ESP32H2
      init_config1.clk_src = ADC_DIGI_CLK_SRC_DEFAULT;
#endif  // USE_ESP32_VARIANT_ESP32C3 || USE_ESP32_VARIANT_ESP32C6 || USE_ESP32_VARIANT_ESP32H2
      ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &this->adc1_handle_));
    }

    adc_oneshot_chan_cfg_t config = {
        .atten = this->attenuation_,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(this->adc1_handle_, this->channel_, &config));

  } else {
    if (this->adc2_handle_ == nullptr) {
      adc_oneshot_unit_init_cfg_t init_config2 = {};  // Zero initialize
      init_config2.unit_id = ADC_UNIT_2;
      init_config2.ulp_mode = ADC_ULP_MODE_DISABLE;
#if USE_ESP32_VARIANT_ESP32C3 || USE_ESP32_VARIANT_ESP32C6 || USE_ESP32_VARIANT_ESP32H2
      init_config2.clk_src = ADC_DIGI_CLK_SRC_DEFAULT;
#endif  // USE_ESP32_VARIANT_ESP32C3 || USE_ESP32_VARIANT_ESP32C6 || USE_ESP32_VARIANT_ESP32H2
      ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config2, &this->adc2_handle_));
    }

    adc_oneshot_chan_cfg_t config = {
        .atten = this->attenuation_,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(this->adc2_handle_, this->channel_, &config));
  }

  // Initialize ADC calibration
  if (this->calibration_handle_ == nullptr) {
    adc_cali_handle_t handle = nullptr;
    adc_unit_t unit_id = this->is_adc1_ ? ADC_UNIT_1 : ADC_UNIT_2;

#if USE_ESP32_VARIANT_ESP32C3 || USE_ESP32_VARIANT_ESP32C6 || USE_ESP32_VARIANT_ESP32S3 || USE_ESP32_VARIANT_ESP32H2
    // RISC-V variants and S3 use curve fitting calibration
    adc_cali_curve_fitting_config_t cali_config = {};  // Zero initialize first
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    cali_config.chan = this->channel_;  // Set chan first as it's the first field in v5.3+
#endif                                  // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    cali_config.unit_id = unit_id;
    cali_config.atten = this->attenuation_;
    cali_config.bitwidth = ADC_BITWIDTH_DEFAULT;

    if (adc_cali_create_scheme_curve_fitting(&cali_config, &handle) == ESP_OK) {
      this->calibration_handle_ = handle;
      ESP_LOGV(TAG, "Using curve fitting calibration");
    }
#else   // USE_ESP32_VARIANT_ESP32C3 || ESP32C6 || ESP32S3 || ESP32H2
    // Other ESP32 variants use line fitting calibration
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = unit_id,
        .atten = this->attenuation_,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_line_fitting(&cali_config, &handle) == ESP_OK) {
      this->calibration_handle_ = handle;
      ESP_LOGV(TAG, "Using line fitting calibration");
    }
#endif  // USE_ESP32_VARIANT_ESP32C3 || ESP32C6 || ESP32S3 || ESP32H2
  }

  this->do_setup_ = false;
}

void ADCSensor::dump_config() {
  LOG_SENSOR("", "ADC Sensor", this);
  LOG_PIN("  Pin: ", this->pin_);
  if (this->autorange_) {
    ESP_LOGCONFIG(TAG, "  Attenuation: auto");
  } else {
    switch (this->attenuation_) {
      case ADC_ATTEN_DB_0:
        ESP_LOGCONFIG(TAG, "  Attenuation: 0db");
        break;
      case ADC_ATTEN_DB_2_5:
        ESP_LOGCONFIG(TAG, "  Attenuation: 2.5db");
        break;
      case ADC_ATTEN_DB_6:
        ESP_LOGCONFIG(TAG, "  Attenuation: 6db");
        break;
      case ADC_ATTEN_DB_12:
        ESP_LOGCONFIG(TAG, "  Attenuation: 12db");
        break;
      default:
        break;
    }
  }
  ESP_LOGCONFIG(TAG, "  Samples: %i", this->sample_count_);
  LOG_UPDATE_INTERVAL(this);
}

float ADCSensor::sample() {
  if (!this->autorange_) {
    uint32_t sum = 0;
    for (uint8_t sample = 0; sample < this->sample_count_; sample++) {
      int raw = -1;
      if (this->is_adc1_) {
        ESP_ERROR_CHECK(adc_oneshot_read(this->adc1_handle_, this->channel_, &raw));
      } else {
        ESP_ERROR_CHECK(adc_oneshot_read(this->adc2_handle_, this->channel_, &raw));
      }
      if (raw == -1) {
        return NAN;
      }
      sum += raw;
    }
    sum = (sum + (this->sample_count_ >> 1)) / this->sample_count_;  // NOLINT(clang-analyzer-core.DivideZero)

    if (this->output_raw_) {
      return sum;
    }

    if (this->calibration_handle_ != nullptr) {
      int voltage_mv;
      ESP_ERROR_CHECK(adc_cali_raw_to_voltage(this->calibration_handle_, sum, &voltage_mv));
      return voltage_mv / 1000.0f;
    }
    return sum * 3.3f / 4095.0f;  // Fallback if no calibration
  }

  // Auto-range mode
  int raw12 = 4095, raw6 = 4095, raw2 = 4095, raw0 = 4095;
  float mv12 = 0, mv6 = 0, mv2 = 0, mv0 = 0;

  // Helper lambda for reading with different attenuations
  auto read_atten = [this](adc_atten_t atten) -> std::pair<int, float> {
    if (this->is_adc1_) {
      adc_oneshot_chan_cfg_t config = {
          .atten = atten,
          .bitwidth = ADC_BITWIDTH_DEFAULT,
      };
      ESP_ERROR_CHECK(adc_oneshot_config_channel(this->adc1_handle_, this->channel_, &config));
      int raw;
      ESP_ERROR_CHECK(adc_oneshot_read(this->adc1_handle_, this->channel_, &raw));
      if (this->calibration_handle_ != nullptr) {
        int voltage_mv;
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(this->calibration_handle_, raw, &voltage_mv));
        return {raw, voltage_mv / 1000.0f};
      }
      return {raw, raw * 3.3f / 4095.0f};
    } else {
      adc_oneshot_chan_cfg_t config = {
          .atten = atten,
          .bitwidth = ADC_BITWIDTH_DEFAULT,
      };
      ESP_ERROR_CHECK(adc_oneshot_config_channel(this->adc2_handle_, this->channel_, &config));
      int raw;
      ESP_ERROR_CHECK(adc_oneshot_read(this->adc2_handle_, this->channel_, &raw));
      if (this->calibration_handle_ != nullptr) {
        int voltage_mv;
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(this->calibration_handle_, raw, &voltage_mv));
        return {raw, voltage_mv / 1000.0f};
      }
      return {raw, raw * 3.3f / 4095.0f};
    }
  };

  auto [raw12_val, mv12_val] = read_atten(ADC_ATTEN_DB_12);
  raw12 = raw12_val;
  mv12 = mv12_val;

  if (raw12 < 4095) {
    auto [raw6_val, mv6_val] = read_atten(ADC_ATTEN_DB_6);
    raw6 = raw6_val;
    mv6 = mv6_val;

    if (raw6 < 4095) {
      auto [raw2_val, mv2_val] = read_atten(ADC_ATTEN_DB_2_5);
      raw2 = raw2_val;
      mv2 = mv2_val;

      if (raw2 < 4095) {
        auto [raw0_val, mv0_val] = read_atten(ADC_ATTEN_DB_0);
        raw0 = raw0_val;
        mv0 = mv0_val;
      }
    }
  }

  if (raw0 == -1 || raw2 == -1 || raw6 == -1 || raw12 == -1) {
    return NAN;
  }

  const int ADC_HALF = 2048;
  uint32_t c12 = std::min(raw12, ADC_HALF);
  uint32_t c6 = ADC_HALF - std::abs(raw6 - ADC_HALF);
  uint32_t c2 = ADC_HALF - std::abs(raw2 - ADC_HALF);
  uint32_t c0 = std::min(4095 - raw0, ADC_HALF);
  uint32_t csum = c12 + c6 + c2 + c0;

  return (mv12 * c12 + mv6 * c6 + mv2 * c2 + mv0 * c0) / csum;
}

}  // namespace adc
}  // namespace esphome

#endif  // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#endif  // USE_ESP32
