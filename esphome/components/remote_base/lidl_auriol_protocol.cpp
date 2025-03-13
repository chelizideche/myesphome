#include "lidl_auriol_protocol.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.weather_station";

uint32_t GET_BITS(const std::vector<uint8_t> &code, uint8_t pos, uint8_t nbits) {
  if (pos + nbits > code.size() * 8) {
    ESP_LOGE(TAG, "GET_BITS out of range");
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

void SET_BITS(std::vector<uint8_t> &code, uint8_t pos, uint8_t nbits, uint32_t c) {
  // TODO
}

// WeatherStationProtocol

bool WeatherStationProtocol::receive_item_(RemoteReceiveData &src, uint32_t high, uint32_t low) const {
  if (!this->inverted_) {
    if (!src.peek_mark(high, 0) || !src.peek_space(low, 1))
      return false;
  } else {
    if (!src.peek_space(high, 0) || !src.peek_mark(low, 1))
      return false;
  }
  src.advance(2);
  return true;
}

bool WeatherStationProtocol::receive_(RemoteReceiveData &src, std::vector<uint8_t> &code) const {
  if (this->receive_item_(src, this->sync_high_, this->sync_low_)) {
    ESP_LOGV(TAG, "sync %" PRId32 " %d %d ", src.get_index(), this->sync_high_, this->sync_low_);
    uint8_t nbits = 0;

    while (nbits < this->nbits_ && src.get_index() < src.size() - 1) {
      size_t pos = !this->reversed_ ? nbits : this->nbits_ - nbits - 1;
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
        if (this->receive_item_(src, this->zero_high_, this->zero_low_) ||
            this->receive_item_(src, this->one_high_, this->one_low_)) {
          ESP_LOGD(TAG, "ignore %" PRIx64 " (%d)", *(uint64_t *) &code[0], nbits);
          return false;
        }
        return true;
      }
    }
  }

  return false;
}

void WeatherStationProtocol::transmit_item_(RemoteTransmitData *dst, uint32_t high, uint32_t low) const {
  if (!this->inverted_) {
    dst->mark(high);
    dst->space(low);
  } else {
    dst->space(high);
    dst->mark(low);
  }
}

void WeatherStationProtocol::transmit_(RemoteTransmitData *dst, const std::vector<uint8_t> &code) const {
  this->transmit_item_(dst, this->sync_high_, this->sync_low_);
  for (uint8_t j = 0; j < this->nbits_; j++) {
    uint8_t i = this->reversed_ ? j : this->nbits_ - j - 1;
    if (code[i >> 3] & (1 << (i & 7))) {
      this->transmit_item_(dst, this->one_high_, this->one_low_);
    } else {
      this->transmit_item_(dst, this->zero_high_, this->zero_low_);
    }
  }
}

optional<WeatherStationData> WeatherStationProtocol::decode(RemoteReceiveData src) {
  this->setup();

  std::vector<uint8_t> code(std::max((this->nbits_ + 7) >> 3, 8));
  int32_t index_limit = std::max(src.size() - (this->nbits_ + 1) * 2, (int32_t) 0);

  //  TODO: limit search to nbits * N, where N is a small number, preferably
  while (src.get_index() <= index_limit) {
    uint32_t index = src.get_index();
    if (!this->receive_(src, code)) {
      src.reset();
      src.advance(index + 1);
      continue;
    }

    WeatherStationData data;
    if (this->transform(code, data)) {
      ESP_LOGD(TAG, "id=%d b=%.0f ch=%d t=%.1f h=%d r=%.1f wd=%.1f ws=%.2f wg=%.2f", data.id, data.battery_level,
               data.channel, data.temperature, data.humidity, data.rain, data.wind_direction_degrees, data.wind_speed,
               data.wind_gust);
      return data;
    }
  }
  return {};
}

void WeatherStationProtocol::encode(RemoteTransmitData *dst, const WeatherStationData &data) {
  this->setup();
  std::vector<uint8_t> code(std::max((this->nbits_ + 7) >> 3, 8), 0);
  if (this->transform(data, code)) {
    ESP_LOGD(TAG, "id=%d b=%.0f ch=%d t=%.1f h=%d r=%.1f wd=%.1f ws=%.2f wg=%.2f", data.id, data.battery_level,
             data.channel, data.temperature, data.humidity, data.rain, data.wind_direction_degrees, data.wind_speed,
             data.wind_gust);
    dst->set_carrier_frequency(38000);  // TODO: channel?
    for (int i = 0; i < this->repeat_; i++) {
      this->transmit_(dst, code);
    }
  }
}

void WeatherStationProtocol::dump(const WeatherStationData &data) {
  ESP_LOGI(TAG, "id=%d b=%.0f ch=%d t=%.1f h=%d r=%.1f wd=%.1f ws=%.2f wg=%.2f", data.id, data.battery_level,
           data.channel, data.temperature, data.humidity, data.rain, data.wind_direction_degrees, data.wind_speed,
           data.wind_gust);
}

// WS2032

void WS2032Protocol::setup() {
  this->sync_high_ = 2000;
  this->sync_low_ = 500;
  this->zero_high_ = 500;
  this->zero_low_ = 500;
  this->one_high_ = 1000;
  this->one_low_ = 500;
  this->inverted_ = false;
  this->nbits_ = 111;
  this->repeat_ = 3;
  this->reversed_ = true;
}

bool WS2032Protocol::transform(const std::vector<uint8_t> &code, WeatherStationData &data) const {
  uint32_t chksum = 0;
  for (uint8_t i = 0, pos = 103; i < 12; i++, pos -= 8) {
    chksum += GET_BITS(code, pos, 8);
  }
  chksum &= 0xff;
  if (GET_BITS(code, 7, 8) != chksum) {
    ESP_LOGV(TAG, "chksum mismatch %x != %x", (uint8_t) GET_BITS(code, 7, 8), chksum);
    return false;
  }

  // TODO: crc8

  // PRE = GET_BITS(code, 103, 8)
  data.id = GET_BITS(code, 87, 16);
  data.battery_level = (GET_BITS(code, 79, 8) & 1) ? 100.0f : 0;
  // FLAG = GET_BITS(code, 79, 8)
  data.wind_direction_degrees = 22.5f * GET_BITS(code, 75, 4);
  data.temperature = (GET_BITS(code, 74, 1) ? -0.1f : 0.1f) * GET_BITS(code, 63, 11);
  data.humidity = GET_BITS(code, 55, 8);
  data.wind_speed = 0.43f * GET_BITS(code, 47, 8);
  data.wind_gust = 0.43f * GET_BITS(code, 39, 8);
  data.rain = (float) GET_BITS(code, 15, 24);  // conversion to mm?
  // ? = GET_BITS(code, 0, 7)

  return true;
}

bool WS2032Protocol::transform(const WeatherStationData &data, std::vector<uint8_t> &code) const {
  ESP_LOGW(TAG, "TODO: encode WS2032");
  return false;
}

// 4LD631

void WS4LD631Protocol::setup() {
  this->sync_high_ = 4000;
  this->sync_low_ = 500;
  this->zero_high_ = 1000;
  this->zero_low_ = 500;
  this->one_high_ = 2000;
  this->one_low_ = 500;
  this->inverted_ = true;
  this->nbits_ = 52;
  this->repeat_ = 7;
  this->reversed_ = true;
}

bool WS4LD631Protocol::transform(const std::vector<uint8_t> &code, WeatherStationData &data) const {
  if ((uint8_t) GET_BITS(code, 24, 4) != 0b1111) {  // unknown, always 0b1111?
    ESP_LOGV(TAG, "[24:27] should be 0b1111");
    return false;
  }

  data.id = (uint16_t) GET_BITS(code, 44, 8);
  data.battery_level = (uint8_t) GET_BITS(code, 43, 1) == 1 ? 100.0f : 0;
  // 0 = (uint8_t) GET_BITS(code, 42, 1); // ?
  data.channel = (uint8_t) GET_BITS(code, 40, 2);
  data.temperature = (float) ((int16_t) (GET_BITS(code, 28, 12) << 4)) / 160;
  data.humidity = (uint8_t) GET_BITS(code, 16, 8);
  data.rain = (float) GET_BITS(code, 0, 16) * 0.242f;
  return true;
}

bool WS4LD631Protocol::transform(const WeatherStationData &data, std::vector<uint8_t> &code) const {
  SET_BITS(code, 44, 8, data.id);
  SET_BITS(code, 43, 1, data.battery_level > 25 ? 1 : 0);  // 25% is pretty dead
  // SET_BITS(code, 42, 1, 0);
  SET_BITS(code, 40, 2, data.channel);
  SET_BITS(code, 28, 12, (uint64_t) ((int16_t) (data.temperature * 160) >> 4));
  SET_BITS(code, 24, 4, 0b1111);
  SET_BITS(code, 16, 8, data.humidity);
  SET_BITS(code, 0, 16, (uint64_t) (data.rain / 0.242f));
  return true;
}

// H10515

void WSH10515Protocol::setup() {
  this->sync_high_ = 500;
  this->sync_low_ = 9000;
  this->zero_high_ = 500;
  this->zero_low_ = 2000;
  this->one_high_ = 500;
  this->one_low_ = 4000;
  this->inverted_ = false;
  this->nbits_ = 36;
  this->repeat_ = 8;
  this->reversed_ = false;
}

bool WSH10515Protocol::transform(const std::vector<uint8_t> &code, WeatherStationData &data) const {
  uint8_t chksum = 0;
  for (int i = 0; i < 8; i++) {
    chksum = (chksum + (uint8_t) GET_BITS(code, i * 4, 4)) & 0b1111;
  }
  chksum = ~chksum & 0b1111;
  if ((uint8_t) GET_BITS(code, 32, 4) != chksum) {
    ESP_LOGV(TAG, "chksum mismatch %x != %x", (uint8_t) GET_BITS(code, 32, 4), chksum);
    return false;
  }
  if (GET_BITS(code, 8, 4) != 0) {  // unknown, always zero(?), sometimes 0b0010
    ESP_LOGV(TAG, "[8:11] should be 0");
    // return false;
  }

  data.id = (uint16_t) GET_BITS(code, 0, 4);  // keeps changing between resets
  data.channel = (uint8_t) ((GET_BITS(code, 4, 1) << 1) | GET_BITS(code, 5, 1));
  if (data.channel == 0) {
    ESP_LOGV(TAG, "channel invalid %d", data.channel);
    return false;
  }
  // ? == GET_BITS(code, 6, 2) // unknown, battery(?)
  data.temperature = (GET_BITS(code, 22, 1) ? -0.1f : 0.1f) * GET_BITS(code, 12, 11);
  data.humidity = 10.0f * GET_BITS(code, 28, 4) + GET_BITS(code, 24, 4);
  return true;
}

bool WSH10515Protocol::transform(const WeatherStationData &data, std::vector<uint8_t> &code) const {
  ESP_LOGW(TAG, "TODO: encode H10515");
  return true;
}

// L08037A

void WSL08037AProtocol::setup() {
  this->sync_high_ = 500;
  this->sync_low_ = 9500;
  this->zero_high_ = 500;
  this->zero_low_ = 2000;
  this->one_high_ = 500;
  this->one_low_ = 4500;
  this->inverted_ = false;
  this->nbits_ = 28;
  this->repeat_ = 8;
  this->reversed_ = true;
}

bool WSL08037AProtocol::transform(const std::vector<uint8_t> &code, WeatherStationData &data) const {
  uint8_t chksum = 0b1111;
  for (int i = 0; i < 6; i++) {
    chksum = (chksum + (uint8_t) GET_BITS(code, i * 4, 4)) & 0b1111;
  }
  if ((uint8_t) GET_BITS(code, 24, 4) != chksum) {
    ESP_LOGV(TAG, "chksum mismatch %x != %x", (uint8_t) GET_BITS(code, 24, 4), chksum);
    return false;
  }

  data.id = (uint16_t) GET_BITS(code, 16, 8);
  data.channel = (uint8_t) GET_BITS(code, 2, 2);
  if (data.channel == 0) {
    ESP_LOGV(TAG, "channel invalid %d", data.channel);
    return false;
  }
  data.temperature = (float) ((int16_t) (GET_BITS(code, 4, 12) << 4)) / 160;
  return true;
}

bool WSL08037AProtocol::transform(const WeatherStationData &data, std::vector<uint8_t> &code) const {
  ESP_LOGW(TAG, "TODO: encode l08037a");
  return true;
}

// NEXUS

void WSNexusProtocol::setup() {
  this->sync_high_ = 500;
  this->sync_low_ = 4000;
  this->zero_high_ = 500;
  this->zero_low_ = 1000;
  this->one_high_ = 500;
  this->one_low_ = 2000;
  this->inverted_ = false;
  this->nbits_ = 36;
  this->repeat_ = 8;
  this->reversed_ = true;
}

bool WSNexusProtocol::transform(const std::vector<uint8_t> &code, WeatherStationData &data) const {
  // 3E806DF2F id=3E, battery ok, 10.9C, 47%
  // 0011 1110 1000 0000 0110 1101 1111 0010 1111

  // the beginning of 4LD631 can be accepted here without chksum, it is only filtered out by checking the presence of
  // additional valid bits before the next sync

  // TODO: chksum?
  if (GET_BITS(code, 8, 4) != 0b1111) {
    ESP_LOGV(TAG, "[8:11] should be 0b1111");
    return false;
  }

  data.id = (uint16_t) GET_BITS(code, 28, 8);
  data.battery_level = (uint8_t) GET_BITS(code, 27, 1) == 1 ? 100.0f : 0;
  data.channel = (uint8_t) GET_BITS(code, 24, 2) + 1;
  data.temperature = (float) ((int16_t) (GET_BITS(code, 12, 12) << 4)) / 160;
  data.humidity = (uint8_t) GET_BITS(code, 0, 8);
  return true;
}

bool WSNexusProtocol::transform(const WeatherStationData &data, std::vector<uint8_t> &code) const {
  ESP_LOGW(TAG, "TODO: encode nexus");
  return true;
}

}  // namespace remote_base
}  // namespace esphome
