#include "lidl_auriol_protocol.h"
#include "rc_switch_protocol.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.lidl_auriol";

// Currently supported:

// Radio-controlled weather station with rain gauge 4-LD6313
// ID(8?)BATTERY(1)0(1)CHANNEL(2)TEMP_C(12)1(4)RAIN(24)
// BATTERY: 1 ok, 0 bad
// TEMP_C: 2's complement * 10

static const uint32_t LIDL_AURIOL_PULSE_LENGTH = 490;
static const RCSwitchBase LIDL_AURIOL_PROTOCOL1 = RCSwitchBase(
  1 * LIDL_AURIOL_PULSE_LENGTH, // sync_high
  8 * LIDL_AURIOL_PULSE_LENGTH, // sync_low
  1 * LIDL_AURIOL_PULSE_LENGTH, // zero_high
  2 * LIDL_AURIOL_PULSE_LENGTH, // zero_low
  1 * LIDL_AURIOL_PULSE_LENGTH, // one_high
  4 * LIDL_AURIOL_PULSE_LENGTH, // one_low
  false); // inverted
static const RCSwitchBase LIDL_AURIOL_PROTOCOL2 = RCSwitchBase(
  8 * LIDL_AURIOL_PULSE_LENGTH, // sync_high
  1 * LIDL_AURIOL_PULSE_LENGTH, // sync_low
  2 * LIDL_AURIOL_PULSE_LENGTH, // zero_high
  1 * LIDL_AURIOL_PULSE_LENGTH, // zero_low
  4 * LIDL_AURIOL_PULSE_LENGTH, // one_high
  1 * LIDL_AURIOL_PULSE_LENGTH, // one_low
  true); // inverted

#define GET_BITS(x, pos, len) (((x) >> (pos)) & ((1 << (len)) - 1))
#define SET_BITS(x, pos, len, val) (x) |= ((uint64_t) ((val) & ((1 << (len)) - 1))) << (pos)

void LidlAuriolProtocol::encode(RemoteTransmitData *dst, const LidlAuriolData &data) {
  uint64_t code = 0;

  SET_BITS(code, 44, 8, data.id);
  SET_BITS(code, 43, 1, data.battery_level);
  //SET_BITS(code, 42, 1, 0);
  SET_BITS(code, 40, 2, data.channel);
  SET_BITS(code, 28, 12, (uint64_t) ((int16_t) (data.temperature * 160) >> 4));
  SET_BITS(code, 24, 4, 0b1111);
  SET_BITS(code, 0, 24, data.rain);

  ESP_LOGD(TAG, "Sending %" PRIx64 " id=%d battery_level=%d channel=%d temperature=%.1f rain=%d", code, data.id,
           data.battery_level, data.channel, data.temperature, data.rain);

  for (int i = 0; i < 7; i++)
    LIDL_AURIOL_PROTOCOL2.transmit(dst, code, 52);
}

optional<LidlAuriolData> LidlAuriolProtocol::decode(RemoteReceiveData src) {
  uint64_t code = 0;
  uint8_t nbits;
  if (!LIDL_AURIOL_PROTOCOL2.decode(src, &code, &nbits)) {
    if (!LIDL_AURIOL_PROTOCOL1.decode(src, &code, &nbits)) {
      return {};
    }
    ESP_LOGD(TAG, "LIDL_AURIOL_PROTOCOL1");
  } else {
    ESP_LOGD(TAG, "LIDL_AURIOL_PROTOCOL2");
  }

  if (GET_BITS(code, 24, 4) != 15)
    return {};

  if (!(nbits >= 44 && nbits <= 52)) {
    // id = 01110000, nbits is 51 only, the leading zero may be missing, 11110000 sends all 52 bits
    // ... could be a receive or protocol setup error
    // ... allow arbitrary number of id bits, for now
    ESP_LOGD(TAG, "Wrong number of bits, received 0x%" PRIx64 " (%d)", code, nbits);
    return {};
  }

  LidlAuriolData data;

  data.code = code;
  data.id = (uint8_t) GET_BITS(code, 44, 8);
  data.battery_level = (uint8_t) GET_BITS(code, 43, 1);
  // 0 = (uint8_t) GET_BITS(code, 42, 1); // ?
  data.channel = (uint8_t) GET_BITS(code, 40, 2);
  data.temperature = (float) ((int16_t) (GET_BITS(code, 28, 12) << 4)) / 160;
  // 1111 = (uint8_t) GET_BITS(code, 24, 4); // ?
  data.rain = (uint32_t) GET_BITS(code, 0, 24);

  return data;
}

void LidlAuriolProtocol::dump(const LidlAuriolData &data) {
  ESP_LOGI(TAG, "Received: %" PRIx64 " id=%d battery_level=%d channel=%d temperature=%.1f rain=%d", data.code,
           data.id, data.battery_level, data.channel, data.temperature, data.rain);
}

}  // namespace remote_base
}  // namespace esphome
