#include "weather_station_protocol.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.weather_station";

// WeatherStationProtocol

optional<WeatherStationData> WeatherStationProtocol::decode(RemoteReceiveData src) {
  this->setup();

  WeatherStationData data;
  std::vector<uint8_t> code(std::max((this->nbits_ + 7) >> 3, 8));

  uint32_t samples = (this->nbits_ + 1) * 2;                                    // sync + nbits
  uint32_t search_end = src.size() > samples ? src.size() - samples + 1 : 0ul;  // last possible sync + 1
  uint32_t search_limit = std::min(search_end, samples);                        // limit search

  while (src.get_index() < search_limit) {
    if (this->receive_item_(src, this->sync_high_, this->sync_low_)) {
      if (this->receive_code_(src, code)) {
        // found something, extend search till the end
        search_limit = search_end;
        // transform may also return false if it needs more packets to complete data
        if (this->transform(code, data)) {
          ESP_LOGD(TAG, "id=%d b=%.0f ch=%d t=%.1f h=%d r=%.2f wd=%.1f ws=%.2f wg=%.2f", data.id, data.battery_level,
                   data.channel, data.temperature, data.humidity, data.rain, data.wind_direction_degrees,
                   data.wind_speed, data.wind_gust);
          return data;
        }
      }
    } else {
      src.advance(1);
    }
  }

  return {};
}

void WeatherStationProtocol::encode(RemoteTransmitData *dst, const WeatherStationData &data) {
  this->setup();
  std::vector<uint8_t> code(std::max((this->nbits_ + 7) >> 3, 8), 0);
  if (this->transform(data, code)) {
    ESP_LOGD(TAG, "id=%d b=%.0f ch=%d t=%.1f h=%d r=%.2f wd=%.1f ws=%.2f wg=%.2f", data.id, data.battery_level,
             data.channel, data.temperature, data.humidity, data.rain, data.wind_direction_degrees, data.wind_speed,
             data.wind_gust);
    dst->set_carrier_frequency(38000);  // TODO: channel?
    for (int i = 0; i < this->repeat_; i++) {
      this->transmit_code_(dst, code);
    }
  }
}

void WeatherStationProtocol::dump(const WeatherStationData &data) {
  ESP_LOGI(TAG, "id=%d b=%.0f ch=%d t=%.1f h=%d r=%.2f wd=%.1f ws=%.2f wg=%.2f", data.id, data.battery_level,
           data.channel, data.temperature, data.humidity, data.rain, data.wind_direction_degrees, data.wind_speed,
           data.wind_gust);
}

bool WeatherStationProtocol::receive_item_(RemoteReceiveData &src, uint32_t high, uint32_t low) const {
  if (!this->is_inverted_()) {
    if (!(this->is_ppm_() ? src.peek_mark_at_most(high, 0) : src.peek_mark(high, 0)))
      return false;
    if (!src.peek_space(low, 1))
      return false;
    src.advance(2);
  } else {
    if (src.get_index() == 0) {
      // assume space at the beginning
      if (!(this->is_ppm_() ? src.peek_mark_at_most(low, 0) : src.peek_mark(low, 0)))
        return false;
      src.advance(1);
    } else {
      if (!src.peek_space(high, 0))
        return false;
      if (!(this->is_ppm_() ? src.peek_mark_at_most(low, 1) : src.peek_mark(low, 1)))
        return false;
      src.advance(2);
    }
  }
  return true;
}

bool WeatherStationProtocol::receive_code_(RemoteReceiveData &src, std::vector<uint8_t> &code) const {
  uint8_t nbits = 0;

  while (nbits < this->nbits_ && src.get_index() < src.size() - 1) {
    size_t pos = !this->is_reversed_() ? nbits : this->nbits_ - nbits - 1;
    uint8_t bit = 1 << (pos & 7);
    uint8_t &dst = code[pos >> 3];

    if (this->receive_item_(src, this->zero_high_, this->zero_low_)) {
      dst &= ~bit;
    } else if (this->receive_item_(src, this->one_high_, this->one_low_)) {
      dst |= bit;
    } else {
      break;
    }
    if (++nbits == this->nbits_) {
      ESP_LOGD(TAG, "receive @%" PRIu32 " %" PRIx64 " (%d)", src.get_index(), *(uint64_t *) &code[0], nbits);
      if (src.get_index() < src.size() - 1) {
        uint32_t index = src.get_index();
        if (this->receive_item_(src, this->zero_high_, this->zero_low_) ||
            this->receive_item_(src, this->one_high_, this->one_low_)) {
          ESP_LOGD(TAG, "ignore %" PRIx64 " (%d)", *(uint64_t *) &code[0], nbits);
          src.reset();
          src.advance(index);
          break;
        }
      }
      return true;
    }
  }

  return false;
}

void WeatherStationProtocol::transmit_item_(RemoteTransmitData *dst, uint32_t high, uint32_t low) const {
  if (!this->is_inverted_()) {
    dst->mark(high);
    dst->space(low);
  } else {
    dst->space(high);
    dst->mark(low);
  }
}

void WeatherStationProtocol::transmit_code_(RemoteTransmitData *dst, const std::vector<uint8_t> &code) const {
  this->transmit_item_(dst, this->sync_high_, this->sync_low_);
  for (uint8_t j = 0; j < this->nbits_; j++) {
    uint8_t i = this->is_reversed_() ? j : this->nbits_ - j - 1;
    if (code[i >> 3] & (1 << (i & 7))) {
      this->transmit_item_(dst, this->one_high_, this->one_low_);
    } else {
      this->transmit_item_(dst, this->zero_high_, this->zero_low_);
    }
  }
}

//

static uint32_t get_bits(const std::vector<uint8_t> &code, uint8_t pos, uint8_t nbits) {
  if (pos + nbits > code.size() * 8) {
    ESP_LOGE(TAG, "get_bits_ out of range");
    return 0;
  }

  uint32_t c = 0;

  if ((pos & 7) == 0) {
    for (uint8_t i = 0; i < nbits; i += 8, pos += 8) {
      c |= (uint32_t) code[pos >> 3] << i;
    }
    if ((nbits & 7) != 0) {
      c &= 0xffffffff >> (32 - nbits);
    }
  } else {
    for (uint8_t i = 0; i < nbits; i++, pos++) {
      if (code[pos >> 3] & (1 << (pos & 7)))
        c |= (uint32_t) 1 << i;
    }
  }

  return c;
}

static void set_bits(std::vector<uint8_t> &code, uint8_t pos, uint8_t nbits, uint32_t c) {
  // TODO
}

// WS2032

// Not sure about the different models, sold on Aliexpress

void WeatherStation2032Protocol::setup() {
  this->sync_high_ = 2000;
  this->sync_low_ = 500;
  this->zero_high_ = 500;
  this->zero_low_ = 500;
  this->one_high_ = 1000;
  this->one_low_ = 500;
  this->nbits_ = 111;
  this->repeat_ = 3;
  this->flags_ = WeatherStationFlag::TypePpm | WeatherStationFlag::Reversed;
}

bool WeatherStation2032Protocol::transform(const std::vector<uint8_t> &code, WeatherStationData &data) const {
  uint32_t chksum = 0;
  for (uint8_t i = 0, pos = 103; i < 12; i++, pos -= 8) {
    chksum += get_bits(code, pos, 8);
  }
  chksum &= 0xff;
  if (get_bits(code, 7, 8) != chksum) {
    ESP_LOGV(TAG, "chksum mismatch %x != %x", (uint8_t) get_bits(code, 7, 8), chksum);
    return false;
  }

  // TODO: crc8

  // PRE = get_bits(code, 103, 8)
  data.id = get_bits(code, 87, 16);
  data.battery_level = (get_bits(code, 79, 8) & 1) ? 100.0f : 0;
  // FLAG = get_bits(code, 79, 8)
  data.wind_direction_degrees = 22.5f * get_bits(code, 75, 4);
  data.temperature = (get_bits(code, 74, 1) ? -0.1f : 0.1f) * get_bits(code, 63, 11);
  data.humidity = get_bits(code, 55, 8);
  data.wind_speed = 0.43f * get_bits(code, 47, 8);
  data.wind_gust = 0.43f * get_bits(code, 39, 8);
  data.rain = (float) get_bits(code, 15, 24);  // conversion to mm?
  // ? = get_bits(code, 0, 7)

  return true;
}

bool WeatherStation2032Protocol::transform(const WeatherStationData &data, std::vector<uint8_t> &code) const {
  ESP_LOGW(TAG, "TODO: encode WS2032");
  return false;
}

// Lidl Auriol 4LD*

// 4-LD5661
// 4-LD5972
// 4-LD6313
// 4-LD6654
// ... there are many models
// some don't have a humidity sensor, that field will be zero

void WeatherStation4LDProtocol::setup() {
  this->sync_high_ = 4000;
  this->sync_low_ = 500;
  this->zero_high_ = 1000;
  this->zero_low_ = 500;
  this->one_high_ = 2000;
  this->one_low_ = 500;
  this->nbits_ = 52;
  this->repeat_ = 7;
  this->flags_ = WeatherStationFlag::TypePpm | WeatherStationFlag::Inverted | WeatherStationFlag::Reversed;
}

bool WeatherStation4LDProtocol::transform(const std::vector<uint8_t> &code, WeatherStationData &data) const {
  if ((uint8_t) get_bits(code, 24, 4) != 0b1111) {  // unknown, always 0b1111?
    ESP_LOGV(TAG, "[24:27] should be 0b1111");
    return false;
  }

  data.id = (uint8_t) get_bits(code, 44, 8);
  data.battery_level = (uint8_t) get_bits(code, 43, 1) == 1 ? 100.0f : 0;
  // 0 = (uint8_t) get_bits(code, 42, 1); // ?
  data.channel = (uint8_t) get_bits(code, 40, 2);
  data.temperature = (float) ((int16_t) (get_bits(code, 28, 12) << 4)) / 160;
  data.humidity = (uint8_t) get_bits(code, 16, 8);
  data.rain = (float) get_bits(code, 0, 16) * 0.242f;
  return true;
}

bool WeatherStation4LDProtocol::transform(const WeatherStationData &data, std::vector<uint8_t> &code) const {
  set_bits(code, 44, 8, data.id);
  set_bits(code, 43, 1, data.battery_level > 25 ? 1 : 0);  // 25% is pretty dead
  // set_bits(code, 42, 1, 0);
  set_bits(code, 40, 2, data.channel);
  set_bits(code, 28, 12, (uint64_t) ((int16_t) (data.temperature * 160) >> 4));
  set_bits(code, 24, 4, 0b1111);
  set_bits(code, 16, 8, data.humidity);
  set_bits(code, 0, 16, (uint64_t) (data.rain / 0.242f));
  return true;
}

// Bresser3CH

void WeatherStationBresser3CHProtocol::setup() {
  this->sync_high_ = 750;
  this->sync_low_ = 750;
  this->zero_high_ = 250;
  this->zero_low_ = 500;
  this->one_high_ = 500;
  this->one_low_ = 250;
  this->nbits_ = 40;
  this->repeat_ = 8;
  this->flags_ = WeatherStationFlag::TypePwm;
}

bool WeatherStationBresser3CHProtocol::transform(const std::vector<uint8_t> &code, WeatherStationData &data) const {
  uint8_t chksum = 0;
  for (int i = 1; i < 5; i++) {
    chksum = (chksum + (uint8_t) get_bits(code, i * 8, 8));
  }
  chksum &= 0xff;
  if ((uint8_t) get_bits(code, 0, 8) != chksum) {
    ESP_LOGV(TAG, "chksum mismatch %x != %x", (uint8_t) get_bits(code, 0, 8), chksum);
    return false;
  }

  data.id = (uint8_t) get_bits(code, 32, 8);
  data.battery_level = (uint8_t) get_bits(code, 31, 1) == 0 ? 100.0f : 0;
  // user initiated transimission = get_bits(code, 30, 1);
  data.channel = (uint8_t) get_bits(code, 28, 2);  // is ch0 a valid option?
  data.temperature = (float) get_bits(code, 16, 12) * 0.1f - 90;
  data.temperature = (data.temperature - 32.0f) * 5.0f / 9.0f;  // F to C
  data.humidity = (uint8_t) get_bits(code, 8, 8);
  return true;
}

bool WeatherStationBresser3CHProtocol::transform(const WeatherStationData &data, std::vector<uint8_t> &code) const {
  ESP_LOGW(TAG, "TODO: encode Bresser3CH");
  return true;
}

// Eurochron

void WeatherStationEurochronProtocol::setup() {
  this->sync_high_ = 500;
  this->sync_low_ = 4000;
  this->zero_high_ = 500;
  this->zero_low_ = 1000;
  this->one_high_ = 500;
  this->one_low_ = 2000;
  this->nbits_ = 36;
  this->repeat_ = 8;
  this->flags_ = WeatherStationFlag::TypePpm | WeatherStationFlag::Reversed;
}

bool WeatherStationEurochronProtocol::transform(const std::vector<uint8_t> &code, WeatherStationData &data) const {
  // this may catch Nexus packets, if the 3rd nibblet is 1111, temperature is simply negative

  uint8_t flags = get_bits(code, 20, 8);
  if ((flags & 0b01101111) != 0) {
    ESP_LOGV(TAG, "[20:23] and [25:26] should be 0");
    return false;
  }

  data.id = (uint8_t) get_bits(code, 28, 8);
  data.battery_level = (uint8_t) get_bits(code, 27, 1) == 0 ? 100.0f : 0;
  // user initiated transimission = get_bits(code, 24, 1);
  data.temperature = (float) ((int16_t) (get_bits(code, 0, 12) << 4)) / 160;
  data.humidity = (uint8_t) get_bits(code, 12, 8);
  return true;
}

bool WeatherStationEurochronProtocol::transform(const WeatherStationData &data, std::vector<uint8_t> &code) const {
  ESP_LOGW(TAG, "TODO: encode Eurochron");
  return true;
}

// H10515

// Lidl H10515/DCF

void WeatherStationH10515Protocol::setup() {
  this->sync_high_ = 500;
  this->sync_low_ = 9000;
  this->zero_high_ = 500;
  this->zero_low_ = 2000;
  this->one_high_ = 500;
  this->one_low_ = 4000;
  this->nbits_ = 36;
  this->repeat_ = 8;
  this->flags_ = WeatherStationFlag::TypePpm;
}

bool WeatherStationH10515Protocol::transform(const std::vector<uint8_t> &code, WeatherStationData &data) const {
  uint8_t chksum = 0;
  for (int i = 0; i < 8; i++) {
    chksum = (chksum + (uint8_t) get_bits(code, i * 4, 4)) & 0b1111;
  }
  chksum = ~chksum & 0b1111;
  if ((uint8_t) get_bits(code, 32, 4) != chksum) {
    ESP_LOGV(TAG, "chksum mismatch %x != %x", (uint8_t) get_bits(code, 32, 4), chksum);
    return false;
  }
  if (get_bits(code, 8, 4) != 0) {  // unknown, always zero(?), sometimes 0b0010
    ESP_LOGV(TAG, "[8:11] should be 0");
    // return false;
  }

  data.id = (uint8_t) get_bits(code, 0, 4);  // keeps changing between resets
  data.channel = (uint8_t) ((get_bits(code, 4, 1) << 1) | get_bits(code, 5, 1));
  if (data.channel == 0) {
    ESP_LOGV(TAG, "channel invalid %d", data.channel);
    return false;
  }
  // ? == get_bits(code, 6, 2) // unknown, battery(?)
  data.temperature = (get_bits(code, 22, 1) ? -0.1f : 0.1f) * get_bits(code, 12, 11);
  data.humidity = 10.0f * get_bits(code, 28, 4) + get_bits(code, 24, 4);
  return true;
}

bool WeatherStationH10515Protocol::transform(const WeatherStationData &data, std::vector<uint8_t> &code) const {
  ESP_LOGW(TAG, "TODO: encode H10515");
  return true;
}

// H13726

// Lidl Auriol H13726
// Ventus WS155,
// Hama EWS 1500,
// Meteoscan W155/W160
// https://github.com/gabest11/datasheet/blob/main/auriol_protocol_v20.pdf
// Unitec W186-F
// https://github.com/merbanan/rtl_433/blob/master/src/devices/alecto.c

void WeatherStationH13726Protocol::setup() {
  this->sync_high_ = 500;
  this->sync_low_ = 9000;
  this->zero_high_ = 500;
  this->zero_low_ = 2000;
  this->one_high_ = 500;
  this->one_low_ = 4000;
  this->nbits_ = 36;
  this->repeat_ = 8;
  this->flags_ = WeatherStationFlag::TypePpm;
}

bool WeatherStationH13726Protocol::transform(const std::vector<uint8_t> &code, WeatherStationData &data) const {
  // only the rain sensor was tested, the other part is beyond repair, it has spent 20 years outside

  uint8_t chksum1 = 0b1111;
  uint8_t chksum2 = 0b0111;

  for (int i = 0; i < 8; i++) {
    uint8_t b = (uint8_t) get_bits(code, i * 4, 4);
    chksum1 = (chksum1 - b) & 0b1111;
    chksum2 = (chksum2 + b) & 0b1111;
  }

  uint8_t chksum = (uint8_t) get_bits(code, 32, 4);

  if (get_bits(code, 9, 2) != 0b11) {  // temperature and humidity
    if (chksum != chksum1) {
      ESP_LOGV(TAG, "chksum1 mismatch %x != %x", chksum, chksum1);
      return false;
    }
    data.temperature = (float) ((int16_t) (get_bits(code, 12, 12) << 4)) / 160;
    data.humidity = 10.0f * get_bits(code, 28, 4) + get_bits(code, 24, 4);
    // TODO: wind packets may also be in this transmission burst, not sure
    // return false;
  } else {
    uint8_t type = get_bits(code, 12, 3);
    if (type == 0b011) {  // rain
      // D000B3602 2.75mm
      // 1101 0000 0000 0000 1011 0011 0110 0000 0010
      if (chksum != chksum2) {
        ESP_LOGV(TAG, "chksum2 mismatch %x != %x", chksum, chksum2);
        return false;
      }
      if (get_bits(code, 15, 1) != 0) {  // always 0?
        return false;
      }
      data.rain = (float) get_bits(code, 16, 16) * 0.25f;
    } else {
      if (chksum != chksum1) {
        ESP_LOGV(TAG, "chksum1 mismatch %x != %x", chksum, chksum1);
        return false;
      }
      if (type == 0b001) {  // wind part 1
        data.wind_speed = (float) get_bits(code, 24, 8) * 0.2f;
        // we still need to see the "wind part 2"
        return false;
      } else if (type == 0b111) {  // wind part 2
        data.wind_direction_degrees = get_bits(code, 15, 9);
        data.wind_gust = (float) get_bits(code, 24, 8) * 0.2f;
      } else {
        return false;
      }
    }
  }

  data.id = get_bits(code, 0, 8);  // keeps changing between resets
  data.battery_level = get_bits(code, 8, 1) == 0 ? 100.0f : 0;
  // user initiated transimission = get_bits(code, 11, 1) == 1;
  return true;
}

bool WeatherStationH13726Protocol::transform(const WeatherStationData &data, std::vector<uint8_t> &code) const {
  ESP_LOGW(TAG, "TODO: encode h13726");
  return true;
}

// L08037A

// Lidl 40782 L08037A

void WeatherStationL08037AProtocol::setup() {
  this->sync_high_ = 500;
  this->sync_low_ = 9500;
  this->zero_high_ = 500;
  this->zero_low_ = 2000;
  this->one_high_ = 500;
  this->one_low_ = 4500;
  this->nbits_ = 28;
  this->repeat_ = 8;
  this->flags_ = WeatherStationFlag::TypePpm | WeatherStationFlag::Reversed;
}

bool WeatherStationL08037AProtocol::transform(const std::vector<uint8_t> &code, WeatherStationData &data) const {
  uint8_t chksum = 0b1111;
  for (int i = 0; i < 6; i++) {
    chksum = (chksum + (uint8_t) get_bits(code, i * 4, 4)) & 0b1111;
  }
  if ((uint8_t) get_bits(code, 24, 4) != chksum) {
    ESP_LOGV(TAG, "chksum mismatch %x != %x", (uint8_t) get_bits(code, 24, 4), chksum);
    return false;
  }

  data.id = (uint8_t) get_bits(code, 16, 8);
  data.channel = (uint8_t) get_bits(code, 2, 2);
  if (data.channel == 0) {
    ESP_LOGV(TAG, "channel invalid %d", data.channel);
    return false;
  }
  data.temperature = (float) ((int16_t) (get_bits(code, 4, 12) << 4)) / 160;
  return true;
}

bool WeatherStationL08037AProtocol::transform(const WeatherStationData &data, std::vector<uint8_t> &code) const {
  ESP_LOGW(TAG, "TODO: encode l08037a");
  return true;
}

// NEXUS

void WeatherStationNexusProtocol::setup() {
  this->sync_high_ = 500;
  this->sync_low_ = 4000;
  this->zero_high_ = 500;
  this->zero_low_ = 1000;
  this->one_high_ = 500;
  this->one_low_ = 2000;
  this->nbits_ = 36;
  this->repeat_ = 8;
  this->flags_ = WeatherStationFlag::TypePpm | WeatherStationFlag::Reversed;
}

bool WeatherStationNexusProtocol::transform(const std::vector<uint8_t> &code, WeatherStationData &data) const {
  // the beginning of Lidl Auriol 4LD can be accepted here without chksum, it is only filtered out by checking the
  // presence of additional valid bits before the next sync

  uint8_t chk = get_bits(code, 8, 4);
  if (chk != 0b1111 && chk != 0b1010) {
    ESP_LOGV(TAG, "[8:11] should be 0b1111 or 0b1010");
    return false;
  }

  data.id = (uint8_t) get_bits(code, 28, 8);
  data.battery_level = (uint8_t) get_bits(code, 27, 1) == 1 ? 100.0f : 0;
  data.channel = (uint8_t) get_bits(code, 24, 2) + 1;
  data.temperature = (float) ((int16_t) (get_bits(code, 12, 12) << 4)) / 160;
  data.humidity = (uint8_t) get_bits(code, 0, 8);
  return true;
}

bool WeatherStationNexusProtocol::transform(const WeatherStationData &data, std::vector<uint8_t> &code) const {
  ESP_LOGW(TAG, "TODO: encode nexus");
  return true;
}

}  // namespace remote_base
}  // namespace esphome
