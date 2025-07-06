#include "climate_ir_panasonic.h"
#include "esphome/core/log.h"

namespace esphome {
namespace climate_ir_panasonic {

static const char *const TAG = "climate.climate_ir_pansonic";

void PanasonicIrClimate::transmit_state() {
  uint8_t state[sizeof(pansonicAC_state_base_)];
  memcpy(state, pansonicAC_state_base_, sizeof(pansonicAC_state_base_));

  switch (this->mode) {
    state[13] &= 0xF0;  // Clear top nibble

    case climate::CLIMATE_MODE_AUTO:
      state[13] |= (0x07) & Mode::AUTO;
      break;

    case climate::CLIMATE_MODE_DRY:
      state[13] |= (0x07) & Mode::DRY;
      break;

    case climate::CLIMATE_MODE_COOL:
      state[13] |= (0x07) & Mode::COOL;
      break;

    case climate::CLIMATE_MODE_HEAT:
      state[13] |= (0x07) & Mode::HEAT;
      break;

    case climate::CLIMATE_MODE_FAN_ONLY:
      state[13] |= (0x07) & Mode::FAN;
      break;

    case climate::CLIMATE_MODE_OFF:
      state[13] &= 1;
      if (this->model_ == Model::CKP) {
        ESP_LOGW(TAG, 'CKP has no declarative power. Only a toggle');
        if (CKP_is_on_) {
          CKP_is_on_ = false;
          state[13] |= 1;
        }
      }
      break;

    default:
      break;
  }

  // Temperature
  state[14] &= 0x3E;  // Clear temperature bits
  if (this->mode == climate::CLIMATE_MODE_FAN_ONLY) {
    // Fan only mode is always 27C (remote sends 27C when in fan mode)
    state[14] |= 27 << 1;
  } else {
    uint8_t temp = roundf(clamp<float>(this->target_temperature, TEMP_MIN, TEMP_MAX));
    state[13] |= temp << 1;
  }

  // Vane swing (and fan speed)
  state[16] = 0;
  if (this->swing_mode == climate::CLIMATE_SWING_OFF) {
    if (this->mode == climate::CLIMATE_MODE_HEAT) {
      state[16] |= VANE_DOWN;  // Default to down to heat from the bottom of the room
    } else {
      state[16] |= VANE_UP;  // Default to up to cool from the top of the room (or dry)
    }
  } else {
    state[16] |= VANE_AUTO;  // Only swings in cool/dry
  }

  // Fan speed
  // Byte16 cleared previously
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_AUTO:
      state[16] |= (FanSpeed::FAN_AUTO & 0x0F) << 4;
      break;

    case climate::CLIMATE_FAN_MINIMUM:
      state[16] |= (FanSpeed::FAN_MINIMUM & 0x0F) << 4;
      break;

    case climate::CLIMATE_FAN_LOW:
      state[16] |= (FanSpeed::FAN_LOW & 0x0F) << 4;
      break;

    case climate::CLIMATE_FAN_MEDIUM:
      state[16] |= (FanSpeed::FAN_MEDIUM & 0x0F) << 4;
      break;

    case climate::CLIMATE_FAN_HIGH:
      state[16] |= (FanSpeed::FAN_HIGH & 0x0F) << 4;
      break;

    case climate::CLIMATE_FAN_MAXIMUM:
      state[16] |= (FanSpeed::FAN_MAXIMUM & 0x0F) << 4;
      break;

    default:
      break;
  }

  state[26] = calcChecksum_(state, sizeof(state));

  // Actually transmit the constructed state
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(CARRIER);

  for (int i = 0; i < sizeof(state) - 8; i++) {
    if (i == 8) {
      data->space(SECTION_GAP);  // First section is 8 bytes. Then the rest of the data follows
    }
    for (int bit = 0; bit < 8; bit++) {
      data->mark(BIT_MARK);
      data->space((byte & (1 << bit)) ? BIT_ONE_SPACE : BIT_ZERO_SPACE);
    }
    byte_to_markspace_(data, state[i], BIT_MARK, BIT_ONE_SPACE, BIT_ZERO_SPACE);
  }
}

uint8_t PanasonicIrClimate::calcChecksum_(uint8_t state[], size_t len) {
  uint16_t sum = 0;
  for (int i = 0; i < len - 1; i++) {
    sum += state[i];
  }
  return sum & 0xFF;
}

bool PanasonicIrClimate::on_receive(remote_base::RemoteReceiveData data) {}

}  // namespace climate_ir_panasonic
}  // namespace esphome
