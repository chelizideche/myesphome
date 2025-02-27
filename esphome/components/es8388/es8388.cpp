#include "es8388.h"
#include "es8388_const.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace es8388 {

static const char *const TAG = "es8388";

// Mark the component as failed; use only in setup
#define ES8388_ERROR_FAILED(func) \
  if (!(func)) { \
    this->mark_failed(); \
    return; \
  }

// Return false; use outside of setup
#define ES8388_ERROR_CHECK(func) \
  if (!(func)) { \
    return false; \
  }

void ES8388::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ES8388...");

  static int dac_power = DAC_OUTPUT_ALL;

  // mute DAC
  this->set_mute_state_(true);

  /* Chip Control and Power Management */
  ES8388_ERROR_FAILED(this->write_byte(ES8388_CONTROL2, 0x50));
  // normal all and power up all
  ES8388_ERROR_FAILED(this->write_byte(ES8388_CHIPPOWER, 0x00));
  // disable DAC and disable Lout/Rout/1/2
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACPOWER, 0xC0));

  // vmidsel/500k ADC/DAC idem
  // Enfr=0,Play&Record Mode,(0x17-both of mic&paly)
  ES8388_ERROR_FAILED(this->write_byte(ES8388_CONTROL1, 0x12));

  // I2S worker mode
  ES8388_ERROR_FAILED(this->write_byte(ES8388_MASTERMODE, 0x00));

  // i2s 16 bits
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACCONTROL1, 0x18));
  // sample freq 256
  // DACFsMode,SINGLE SPEED; DACFsRatio,256
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACCONTROL2, 0x02));
  // 0x00 audio on LIN1&RIN1,  0x09 LIN2&RIN2
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACCONTROL16, 0x00));
  // only left DAC to left mixer enable 0db
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACCONTROL17, 0x90));
  // only right DAC to right mixer enable 0db
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACCONTROL20, 0x90));
  // set internal ADC and DAC use the same LRCK clock, ADC LRCK as internal LRCK
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACCONTROL21, 0x80));
  // vroi=0
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACCONTROL23, 0x00));

  dac_power = 0;
  dac_power = DAC_OUTPUT_LOUT1 | DAC_OUTPUT_LOUT2 | DAC_OUTPUT_ROUT1 | DAC_OUTPUT_ROUT2;

  // DAC power-up LOUT1/ROUT1 and LOUT2/ROUT2 enabled
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACPOWER, dac_power));

  // power down adc and line in
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCPOWER, 0xFF));

  //@nightdav
  ES8388_ERROR_FAILED(
      this->write_byte(ES8388_ADCCONTROL1, 0x00));  // +21dB : recommended value for ALC & voice recording

  // Mic input : L-R differential
  // ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL2, ADC_INPUT_DIFFERENCE));
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL2, ADC_INPUT_LINPUT2_RINPUT2));
  // ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL2,ADC_INPUT_LINPUT1_RINPUT1 ));  //original

  // set to Mono Right
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL3, 0x02));

  // I2S 16 Bits length and I2S serial audio data format
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL4, 0x0d));
  // ADCFsMode,singel SPEED,RATIO=256
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL5, 0x02));

  // ADC Volume
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL8, 0x00));
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL9, 0x00));

  // ALC OFF  original

  // Power on ADC, enable LIN&RIN, power off MICBIAS, and set int1lp to low power mode
  // enable dac
  // this->write_byte(ES8388_ADCPOWER, 0x09);
  // this->write_byte(ES8388_DACCONTROL21, 0x80);

  //@nightDav
  // ALC Config (as recommended by ES8388 user guide for voice recording)

  // Reg 0x12 = 0xe2 (ALC enable, PGA Max. Gain=23.5dB, Min. Gain=0dB)
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL10, 0xe2));

  // Reg 0x13 = 0xa0 (ALC Target=-1.5dB, ALC Hold time =0 mS)
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL11, 0xa0));
  // Reg 0x14 = 0x12(Decay time =820uS , Attack time = 416 uS)
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL12, 0x12));

  // Reg 0x15 = 0x06(ALC mode)
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL13, 0x06));

  // Reg 0x16 = 0xc3(nose gate = -40.5dB, NGG = 0x01(mute ADC))
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL14, 0xc3));

  // Power on ADC
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACCONTROL21, 0x80));

  // Start state machine
  ES8388_ERROR_FAILED(this->write_byte(ES8388_CHIPPOWER, 0xF0));
  delay(1);
  ES8388_ERROR_FAILED(this->write_byte(ES8388_CHIPPOWER, 0x00));

  // DAC volume max
  // Set initial volume
  this->set_volume(0.75);  // 0.75 = 0xBF = 0dB

  // DAC volume max
  // Set L1 R1 L2 R2 volume. 0x00: -30dB, 0x1E: 0dB, 0x21: 3dB , 0x1C: 28db
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACCONTROL24, 0x1C));
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACCONTROL25, 0x1C));

  // Set Headphone volume max  0x00: -30dB, 0x1E: 0dB, 0x21: 3dB , 0x1C: 28db
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACCONTROL26, 0x1C));
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACCONTROL27, 0x1C));

  this->set_mute_state_(false);

  // unmute ADC with fade in
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCCONTROL7, 0x60));
  // unmute DAC with fade in
  ES8388_ERROR_FAILED(this->write_byte(ES8388_DACCONTROL3, 0x20));

  // Power on ADC, Enable LIN&RIN, Power off MICBIAS, set int1lp to low power mode
  ES8388_ERROR_FAILED(this->write_byte(ES8388_ADCPOWER, 0x09));
}

void ES8388::dump_config() {
  ESP_LOGCONFIG(TAG, "ES8388 Audio Codec:");

  if (this->is_failed()) {
    ESP_LOGCONFIG(TAG, "  Failed to initialize");
    return;
  }
}

bool ES8388::set_volume(float volume) {
  volume = clamp(volume, 0.0f, 1.0f);
  uint8_t reg = remap<uint8_t, float>(volume, 0.0f, 1.0f, -96, 0);
  ESP_LOGV(TAG, "Setting ES8388_DACCONTROL4 to %u (volume: %f)", reg, volume);
  ESP_LOGV(TAG, "Setting ES8388_DACCONTROL5 to %u (volume: %f)", reg, volume);
  ES8388_ERROR_CHECK(this->write_byte(ES8388_DACCONTROL4, reg));
  ES8388_ERROR_CHECK(this->write_byte(ES8388_DACCONTROL5, reg));

  return true;
}

float ES8388::volume() {
  uint8_t reg;
  ES8388_ERROR_CHECK(this->read_byte(ES8388_DACCONTROL4, &reg));
  return remap<float, uint8_t>(reg, -96, 0, 0.0f, 1.0f);
}

bool ES8388::set_mute_state_(bool mute_state) {
  uint8_t reg13;

  this->is_muted_ = mute_state;

  if (!this->read_byte(ES8388_DACCONTROL3, &reg13)) {
    return false;
  }

  ESP_LOGV(TAG, "Read ES8388_DACCONTROL3: %u", reg13);

  if (mute_state) {
    reg13 |= BIT(1) | BIT(2);
  } else {
    reg13 &= ~(BIT(1) | BIT(2));
  }

  ESP_LOGV(TAG, "Setting ES8388_DACCONTROL3 to %u (muted: %s)", reg13, YESNO(mute_state));
  return this->write_byte(ES8388_DACCONTROL3, reg13);
}

}  // namespace es8388
}  // namespace esphome
