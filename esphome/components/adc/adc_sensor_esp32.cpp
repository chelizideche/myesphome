#ifdef USE_ESP32

#include "adc_sensor.h"
#include "esphome/core/log.h"


#ifdef USE_ZEPHYR
#include "hal/nrf_saadc.h"
#endif

namespace esphome {
namespace adc {

static const char *const TAG = "adc.esp32";

static const adc_bits_width_t ADC_WIDTH_MAX_SOC_BITS = static_cast<adc_bits_width_t>(ADC_WIDTH_MAX - 1);

#ifndef SOC_ADC_RTC_MAX_BITWIDTH
#if USE_ESP32_VARIANT_ESP32S2
static const int32_t SOC_ADC_RTC_MAX_BITWIDTH = 13;
#else
static const int32_t SOC_ADC_RTC_MAX_BITWIDTH = 12;
#endif  // USE_ESP32_VARIANT_ESP32S2
#endif  // SOC_ADC_RTC_MAX_BITWIDTH

static const int ADC_MAX = (1 << SOC_ADC_RTC_MAX_BITWIDTH) - 1;
static const int ADC_HALF = (1 << SOC_ADC_RTC_MAX_BITWIDTH) >> 1;

void ADCSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADC '%s'...", this->get_name().c_str());

  if (this->channel1_ != ADC1_CHANNEL_MAX) {
    adc1_config_width(ADC_WIDTH_MAX_SOC_BITS);
    if (!this->autorange_) {
      adc1_config_channel_atten(this->channel1_, this->attenuation_);
    }
  } else if (this->channel2_ != ADC2_CHANNEL_MAX) {
    if (!this->autorange_) {
      adc2_config_channel_atten(this->channel2_, this->attenuation_);
    }
  }

  for (int32_t i = 0; i <= ADC_ATTEN_DB_12_COMPAT; i++) {
    auto adc_unit = this->channel1_ != ADC1_CHANNEL_MAX ? ADC_UNIT_1 : ADC_UNIT_2;
    auto cal_value = esp_adc_cal_characterize(adc_unit, (adc_atten_t) i, ADC_WIDTH_MAX_SOC_BITS,
                                              1100,  // default vref
                                              &this->cal_characteristics_[i]);
    switch (cal_value) {
      case ESP_ADC_CAL_VAL_EFUSE_VREF:
        ESP_LOGV(TAG, "Using eFuse Vref for calibration");
        break;
      case ESP_ADC_CAL_VAL_EFUSE_TP:
        ESP_LOGV(TAG, "Using two-point eFuse Vref for calibration");
        break;
      case ESP_ADC_CAL_VAL_DEFAULT_VREF:
      default:
        break;
    }
  }

#ifdef USE_ZEPHYR
  if (!adc_is_ready_dt(adc_channel_)) {
    ESP_LOGE(TAG, "ADC controller device %s not ready", adc_channel_->dev->name);
    return;
  }

  auto err = adc_channel_setup_dt(adc_channel_);
  if (err < 0) {
    ESP_LOGE(TAG, "Could not setup channel %s (%d)", adc_channel_->dev->name, err);
    return;
  }
#endif
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
      case ADC_ATTEN_DB_12_COMPAT:
        ESP_LOGCONFIG(TAG, "  Attenuation: 12db");
        break;
      default:  // This is to satisfy the unused ADC_ATTEN_MAX
        break;
    }
  }

#ifdef USE_ZEPHYR
  ESP_LOGCONFIG(TAG, "  Name: %s, channel_id: %d, vref_mv: %d, resolution %d, oversampling %d", adc_channel_->dev->name,
                adc_channel_->channel_id, adc_channel_->vref_mv, adc_channel_->resolution, adc_channel_->oversampling);

  auto gain = [](enum adc_gain gain) {
    switch (gain) {
      case ADC_GAIN_1_6: /**< x 1/6. */
        return "1/6";
      case ADC_GAIN_1_5: /**< x 1/5. */
        return "1/5";
      case ADC_GAIN_1_4: /**< x 1/4. */
        return "1/4";
      case ADC_GAIN_1_3: /**< x 1/3. */
        return "1/3";
      case ADC_GAIN_2_5: /**< x 2/5. */
        return "2/5";
      case ADC_GAIN_1_2: /**< x 1/2. */
        return "1/2";
      case ADC_GAIN_2_3: /**< x 2/3. */
        return "2/3";
      case ADC_GAIN_4_5: /**< x 4/5. */
        return "4/5";
      case ADC_GAIN_1: /**< x 1. */
        return "1";
      case ADC_GAIN_2: /**< x 2. */
        return "2";
      case ADC_GAIN_3: /**< x 3. */
        return "3";
      case ADC_GAIN_4: /**< x 4. */
        return "4";
      case ADC_GAIN_6: /**< x 6. */
        return "6";
      case ADC_GAIN_8: /**< x 8. */
        return "8";
      case ADC_GAIN_12: /**< x 12. */
        return "12";
      case ADC_GAIN_16: /**< x 16. */
        return "16";
      case ADC_GAIN_24: /**< x 24. */
        return "24";
      case ADC_GAIN_32: /**< x 32. */
        return "32";
      case ADC_GAIN_64: /**< x 64. */
        return "64";
      case ADC_GAIN_128: /**< x 128. */
        return "128";
    }
    return "undefined";
  };

  auto reference = [](enum adc_reference reference) {
    switch (reference) {
      case ADC_REF_VDD_1:
        return "VDD";
      case ADC_REF_VDD_1_2:
        return "VDD/2";
      case ADC_REF_VDD_1_3:
        return "VDD/2";
      case ADC_REF_VDD_1_4:
        return "VDD/4";
      case ADC_REF_INTERNAL:
        return "INTERNAL";
      case ADC_REF_EXTERNAL0:
        return "External, input 0";
      case ADC_REF_EXTERNAL1:
        return "External, input 1";
    }
    return "undefined";
  };

  auto input = [](uint8_t input) {
    switch (input) {
      case NRF_SAADC_INPUT_AIN0:
        return "AIN0";
      case NRF_SAADC_INPUT_AIN1:
        return "AIN1";
      case NRF_SAADC_INPUT_AIN2:
        return "AIN2";
      case NRF_SAADC_INPUT_AIN3:
        return "AIN3";
      case NRF_SAADC_INPUT_AIN4:
        return "AIN4";
      case NRF_SAADC_INPUT_AIN5:
        return "AIN5";
      case NRF_SAADC_INPUT_AIN6:
        return "AIN6";
      case NRF_SAADC_INPUT_AIN7:
        return "AIN7";
      case NRF_SAADC_INPUT_VDD:
        return "VDD";
      case NRF_SAADC_INPUT_VDDHDIV5:
        return "VDDHDIV5";
    }
    return "undefined";
  };

  ESP_LOGCONFIG(TAG, "  Gain: %s, reference: %s, acquisition_time: %d, differential %s",
                gain(adc_channel_->channel_cfg.gain), reference(adc_channel_->channel_cfg.reference),
                adc_channel_->channel_cfg.acquisition_time, YESNO(adc_channel_->channel_cfg.differential));
  if (adc_channel_->channel_cfg.differential) {
    ESP_LOGCONFIG(TAG, "  Input positive: %s, negative: %s", input(adc_channel_->channel_cfg.input_positive),
                  input(adc_channel_->channel_cfg.input_negative));
  } else {
    ESP_LOGCONFIG(TAG, "  Input positive: %s", input(adc_channel_->channel_cfg.input_positive));
  }
#endif

  ESP_LOGCONFIG(TAG, "  Samples: %i", this->sample_count_);
  ESP_LOGCONFIG(TAG, "  Sampling mode: %s", LOG_STR_ARG(sampling_mode_to_str(this->sampling_mode_)));
  LOG_UPDATE_INTERVAL(this);
}

float ADCSensor::sample() {
  if (!this->autorange_) {
    auto aggr = Aggregator(this->sampling_mode_);

    for (uint8_t sample = 0; sample < this->sample_count_; sample++) {
      int raw = -1;
      if (this->channel1_ != ADC1_CHANNEL_MAX) {
        raw = adc1_get_raw(this->channel1_);
      } else if (this->channel2_ != ADC2_CHANNEL_MAX) {
        adc2_get_raw(this->channel2_, ADC_WIDTH_MAX_SOC_BITS, &raw);
      }
      if (raw == -1) {
        return NAN;
      }

      aggr.add_sample(raw);
    }
    if (this->output_raw_) {
      return aggr.aggregate();
    }
    uint32_t mv =
        esp_adc_cal_raw_to_voltage(aggr.aggregate(), &this->cal_characteristics_[(int32_t) this->attenuation_]);
    return mv / 1000.0f;
  }

  int raw12 = ADC_MAX, raw6 = ADC_MAX, raw2 = ADC_MAX, raw0 = ADC_MAX;

  if (this->channel1_ != ADC1_CHANNEL_MAX) {
    adc1_config_channel_atten(this->channel1_, ADC_ATTEN_DB_12_COMPAT);
    raw12 = adc1_get_raw(this->channel1_);
    if (raw12 < ADC_MAX) {
      adc1_config_channel_atten(this->channel1_, ADC_ATTEN_DB_6);
      raw6 = adc1_get_raw(this->channel1_);
      if (raw6 < ADC_MAX) {
        adc1_config_channel_atten(this->channel1_, ADC_ATTEN_DB_2_5);
        raw2 = adc1_get_raw(this->channel1_);
        if (raw2 < ADC_MAX) {
          adc1_config_channel_atten(this->channel1_, ADC_ATTEN_DB_0);
          raw0 = adc1_get_raw(this->channel1_);
        }
      }
    }
  } else if (this->channel2_ != ADC2_CHANNEL_MAX) {
    adc2_config_channel_atten(this->channel2_, ADC_ATTEN_DB_12_COMPAT);
    adc2_get_raw(this->channel2_, ADC_WIDTH_MAX_SOC_BITS, &raw12);
    if (raw12 < ADC_MAX) {
      adc2_config_channel_atten(this->channel2_, ADC_ATTEN_DB_6);
      adc2_get_raw(this->channel2_, ADC_WIDTH_MAX_SOC_BITS, &raw6);
      if (raw6 < ADC_MAX) {
        adc2_config_channel_atten(this->channel2_, ADC_ATTEN_DB_2_5);
        adc2_get_raw(this->channel2_, ADC_WIDTH_MAX_SOC_BITS, &raw2);
        if (raw2 < ADC_MAX) {
          adc2_config_channel_atten(this->channel2_, ADC_ATTEN_DB_0);
          adc2_get_raw(this->channel2_, ADC_WIDTH_MAX_SOC_BITS, &raw0);
        }
      }
    }
  }

  if (raw0 == -1 || raw2 == -1 || raw6 == -1 || raw12 == -1) {
    return NAN;
  }

  uint32_t mv12 = esp_adc_cal_raw_to_voltage(raw12, &this->cal_characteristics_[(int32_t) ADC_ATTEN_DB_12_COMPAT]);
  uint32_t mv6 = esp_adc_cal_raw_to_voltage(raw6, &this->cal_characteristics_[(int32_t) ADC_ATTEN_DB_6]);
  uint32_t mv2 = esp_adc_cal_raw_to_voltage(raw2, &this->cal_characteristics_[(int32_t) ADC_ATTEN_DB_2_5]);
  uint32_t mv0 = esp_adc_cal_raw_to_voltage(raw0, &this->cal_characteristics_[(int32_t) ADC_ATTEN_DB_0]);

  uint32_t c12 = std::min(raw12, ADC_HALF);
  uint32_t c6 = ADC_HALF - std::abs(raw6 - ADC_HALF);
  uint32_t c2 = ADC_HALF - std::abs(raw2 - ADC_HALF);
  uint32_t c0 = std::min(ADC_MAX - raw0, ADC_HALF);
  uint32_t csum = c12 + c6 + c2 + c0;

  uint32_t mv_scaled = (mv12 * c12) + (mv6 * c6) + (mv2 * c2) + (mv0 * c0);
  return mv_scaled / (float) (csum * 1000U);
}

#ifdef USE_ZEPHYR
float ADCSensor::sample() {
  int16_t buf = 0;
  struct adc_sequence sequence = {
      .buffer = &buf,
      /* buffer size in bytes, not number of samples */
      .buffer_size = sizeof(buf),
  };
  int32_t val_mv;

  auto err = adc_sequence_init_dt(adc_channel_, &sequence);
  if (err < 0) {
    ESP_LOGE(TAG, "Could sequence init %s (%d)", adc_channel_->dev->name, err);
    return 0.0;
  }

  err = adc_read(adc_channel_->dev, &sequence);
  if (err < 0) {
    ESP_LOGE(TAG, "Could not read %s (%d)", adc_channel_->dev->name, err);
    return 0.0;
  }

  /*
   * If using differential mode, the 16 bit value
   * in the ADC sample buffer should be a signed 2's
   * complement value.
   */
  if (adc_channel_->channel_cfg.differential) {
    val_mv = (int32_t) ((int16_t) buf);
  } else {
    val_mv = (int32_t) buf;
    // https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/0ed4d9ffc674ae407be7cacf5696a02f5e789861/cores/nRF5/wiring_analog_nRF52.c#L222
    if (val_mv < 0) {
      val_mv = 0;
    }
  }

  if (output_raw_) {
    return val_mv;
  }

  err = adc_raw_to_millivolts_dt(adc_channel_, &val_mv);
  /* conversion to mV may not be supported, skip if not */
  if (err < 0) {
    ESP_LOGE(TAG, "Value in mV not available %s (%d)", adc_channel_->dev->name, err);
    return 0.0;
  }

  return val_mv / 1000.0f;
}
#endif

}  // namespace adc
}  // namespace esphome

#endif  // USE_ESP32
