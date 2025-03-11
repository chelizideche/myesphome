#include "lidl_auriol_protocol.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.lidl_auriol";

#define GET_BITS(x, pos, len) (((x) >> (pos)) & ((1 << (len)) - 1))
#define SET_BITS(x, pos, len, val) (x) |= ((uint64_t) ((val) & ((1 << (len)) - 1))) << (pos)

// temperature and rain sensor
static const char *const MODEL_4LD631 = "4LD631";
// none of my cc1101s can receive this, but a simple srx882 can
static const char *const MODEL_H10515_DCF = "H10515_DCF";
// TODO: temperature, humidity, wind, rain, from the year 2005, sold under many names
// corroded away on the tree, cannot test it anymore, sadly...
static const char *const MODEL_H13726A = "H13726A";
// temperature only (aka TFA-Pool)
static const char *const MODEL_L08037A = "L08037A";
// temperature, humidity (not auriol, but very similar packets)
static const char *const MODEL_NEXUS = "NEXUS";

struct PROTOCOL {
  const char *model;
  uint8_t nbits;
  uint8_t repeat;
  bool reversed;
  bool (*decode)(LidlAuriolData &data);
  bool (*encode)(const LidlAuriolData &data, uint64_t &code);
};

struct TIMING {
  struct {
    uint32_t pulse_length;
    uint8_t sync_high;
    uint8_t sync_low;
    uint8_t zero_high;
    uint8_t zero_low;
    uint8_t one_high;
    uint8_t one_low;
    bool inverted;
  };

  std::vector<struct PROTOCOL> protocols;

  bool receive_item(RemoteReceiveData &src, uint32_t high, uint32_t low) const {
    if (!this->inverted) {
      if (!src.peek_mark(pulse_length * high, 0) || !src.peek_space(pulse_length * low, 1))
        return false;
    } else {
      if (!src.peek_space(pulse_length * high, 0) || !src.peek_mark(pulse_length * low, 1))
        return false;
    }
    src.advance(2);
    return true;
  }

  bool receive(RemoteReceiveData &src, uint8_t max_nbits, uint64_t *out_code, uint8_t *out_nbits) const {
    while (src.get_index() < src.size()) {
      // ESP_LOGD(TAG, "sync %" PRIu32 " %" PRId32 " %" PRId32 "", src.get_index(), src.peek(0), src.peek(1));
      if (this->receive_item(src, this->sync_high, this->sync_low)) {
        uint32_t index = src.get_index();

        *out_code = 0;
        *out_nbits = 0;

        for (*out_nbits = 0; *out_nbits < max_nbits; *out_nbits = *out_nbits + 1) {
          if (this->receive_item(src, this->zero_high, this->zero_low)) {
            *out_code <<= 1;
          } else if (this->receive_item(src, this->one_high, this->one_low)) {
            *out_code <<= 1;
            *out_code |= 1;
          } else {
            break;
          }
        }

        src.reset();
        src.advance(index);

        return true;
      }

      src.advance(1);
    }

    return false;
  }

  void transmit_item(RemoteTransmitData *dst, uint32_t high, uint32_t low) const {
    if (!this->inverted) {
      dst->mark(high);
      dst->space(low);
    } else {
      dst->space(high);
      dst->mark(low);
    }
  }

  void transmit(RemoteTransmitData *dst, uint64_t code, uint8_t nbits) const {
    dst->set_carrier_frequency(38000);  // TODO
    this->transmit_item(dst, this->sync_high, this->sync_low);
    for (int16_t i = nbits - 1; i >= 0; i--) {
      if (code & ((uint64_t) 1 << i)) {
        this->transmit_item(dst, this->one_high, this->one_low);
      } else {
        this->transmit_item(dst, this->zero_high, this->zero_low);
      }
    }
  }
};

static bool decode_4ld631(LidlAuriolData &data) {
  if ((uint8_t) GET_BITS(data.code, 24, 4) != 0b1111) {  // unknown, always 0b1111?
    ESP_LOGV(TAG, "[24:27] should be 0b1111");
    return false;
  }

  data.id = (uint16_t) GET_BITS(data.code, 44, 8);
  data.battery_level = (uint8_t) GET_BITS(data.code, 43, 1) == 1 ? 100.0f : 0;
  // 0 = (uint8_t) GET_BITS(code, 42, 1); // ?
  data.channel = (uint8_t) GET_BITS(data.code, 40, 2);
  data.temperature = (float) ((int16_t) (GET_BITS(data.code, 28, 12) << 4)) / 160;
  data.humidity = (uint8_t) GET_BITS(data.code, 16, 8);
  data.rain = (float) GET_BITS(data.code, 0, 16) * 0.242f;
  return true;
}

static bool encode_4ld631(const LidlAuriolData &data, uint64_t &code) {
  code = 0;
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

static bool decode_h10515_dcf(LidlAuriolData &data) {
  uint8_t chksum = 0;
  for (int i = 0; i < 8; i++) {
    chksum = (chksum + (uint8_t) GET_BITS(data.code, i * 4, 4)) & 0b1111;
  }
  chksum = ~chksum & 0b1111;
  if ((uint8_t) GET_BITS(data.code, 32, 4) != chksum) {
    ESP_LOGV(TAG, "chksum mismatch %x != %x", (uint8_t) GET_BITS(data.code, 32, 4), chksum);
    return false;
  }
  if (GET_BITS(data.code, 8, 4) != 0) {  // unknown, always zero(?), sometimes 0b0010
    ESP_LOGV(TAG, "[8:11] should be 0");
    // return false;
  }

  data.id = (uint16_t) GET_BITS(data.code, 0, 4);  // keeps changing between resets
  data.channel = (uint8_t) ((GET_BITS(data.code, 4, 1) << 1) | GET_BITS(data.code, 5, 1));
  if (data.channel == 0) {
    ESP_LOGV(TAG, "channel invalid %d", data.channel);
    return false;
  }
  // ? == GET_BITS(data.code, 6, 2) // unknown, battery(?)
  data.temperature = (GET_BITS(data.code, 22, 1) ? -0.1 : 0.1) * GET_BITS(data.code, 12, 11);
  data.humidity = 10.0f * GET_BITS(data.code, 28, 4) + GET_BITS(data.code, 24, 4);
  return true;
}

static bool encode_h10515_dcf(const LidlAuriolData &data, uint64_t &code) {
  ESP_LOGD(TAG, "TODO: encode_h10515_dcf");
  return false;
}

static bool decode_h13726a(LidlAuriolData &data) {
  uint8_t chksum = 0b1111;
  for (int i = 0; i < 8; i++) {
    chksum = (chksum - (uint8_t) GET_BITS(data.code, i * 4, 4)) & 0b1111;
  }
  if ((uint8_t) GET_BITS(data.code, 32, 4) != chksum) {
    ESP_LOGV(TAG, "chksum mismatch %x != %x", (uint8_t) GET_BITS(data.code, 32, 4), chksum);
    return false;
  }

  data.id = (uint16_t) GET_BITS(data.code, 0, 8);
  data.temperature = (float) ((int16_t) (GET_BITS(data.code, 12, 12) << 4)) / 160;
  data.humidity = 10.0f * GET_BITS(data.code, 28, 4) + GET_BITS(data.code, 24, 4);
  return true;
}

static bool encode_h13726a(const LidlAuriolData &data, uint64_t &code) {
  ESP_LOGD(TAG, "TODO: encode_h13726a");
  return false;
}

static bool decode_l08037a(LidlAuriolData &data) {
  uint8_t chksum = 0b1111;
  for (int i = 0; i < 6; i++) {
    chksum = (chksum + (uint8_t) GET_BITS(data.code, i * 4, 4)) & 0b1111;
  }
  if ((uint8_t) GET_BITS(data.code, 24, 4) != chksum) {
    ESP_LOGV(TAG, "chksum mismatch %x != %x", (uint8_t) GET_BITS(data.code, 24, 4), chksum);
    return false;
  }

  data.id = (uint16_t) GET_BITS(data.code, 16, 8);
  data.channel = (uint8_t) GET_BITS(data.code, 2, 2);
  if (data.channel == 0) {
    ESP_LOGV(TAG, "channel invalid %d", data.channel);
    return false;
  }
  data.temperature = (float) ((int16_t) (GET_BITS(data.code, 4, 12) << 4)) / 160;
  return true;
}

static bool encode_l08037a(const LidlAuriolData &data, uint64_t &code) {
  ESP_LOGD(TAG, "TODO: encode_l08037a");
  return false;
}

static bool decode_nexus(LidlAuriolData &data) {
  // 3E806DF2F id=3E, battery ok, 10.9C, 47%
  // 0011 1110 1000 0000 0110 1101 1111 0010 1111
  // TODO: chksum?
  if (GET_BITS(data.code, 8, 4) != 0b1111) {
    ESP_LOGV(TAG, "[8:11] should be 0b1111");
    return false;
  }

  data.id = (uint16_t) GET_BITS(data.code, 28, 8);
  data.battery_level = (uint8_t) GET_BITS(data.code, 27, 1) == 1 ? 100.0f : 0;
  data.channel = (uint8_t) GET_BITS(data.code, 24, 2) + 1;
  data.temperature = (float) ((int16_t) (GET_BITS(data.code, 12, 12) << 4)) / 160;
  data.humidity = (uint8_t) GET_BITS(data.code, 0, 8);
  return true;
}

static bool encode_nexus(const LidlAuriolData &data, uint64_t &code) {
  ESP_LOGD(TAG, "TODO: encode_nexus");
  return false;
}

static bool encode_dummy(const LidlAuriolData &data, uint64_t &code) {
  ESP_LOGD(TAG, "TODO: encode_dummy");
  return true;
}

static struct TIMING TIMINGS[] = {
    {
        {500, 8, 1, 2, 1, 4, 1, true},
        {
            {MODEL_4LD631, 52, 7, false, decode_4ld631, encode_4ld631},
        },
    },
    {
        {500, 1, 8, 1, 2, 1, 4, false},
        {
            // {MODEL_4LD631, 52, 7, false, decode_4ld631, encode_4ld631},
            {MODEL_NEXUS, 36, 8, false, decode_nexus, encode_dummy},
            {MODEL_H13726A, 36, 8, false, decode_h13726a, encode_dummy},
        },
    },
    {
        {500, 1, 18, 1, 4, 1, 8, false},
        {
            {MODEL_H10515_DCF, 36, 8, true, decode_h10515_dcf, encode_dummy},
        },
    },
    {
        {500, 1, 19, 1, 4, 1, 9, false},
        {
            {MODEL_L08037A, 28, 8, false, decode_l08037a, encode_dummy},
        },
    },
};

void LidlAuriolProtocol::encode(RemoteTransmitData *dst, const LidlAuriolData &data) {
  for (const auto &t : TIMINGS) {
    for (const auto &p : t.protocols) {
      if (data.model == p.model) {
        uint64_t code = 0;
        if (p.encode(data, code)) {
          ESP_LOGD(TAG, "%s %" PRIx64 " id=%d b=%.0f ch=%d t=%.1f h=%d r=%.1f wd=%d ws=%.2f wg=%.2f",
                   data.model.c_str(), code, data.id, data.battery_level, data.channel, data.temperature, data.humidity,
                   data.rain, data.wind_direction_degrees, data.wind_speed, data.wind_gust);

          for (int i = 0; i < p.repeat; i++) {
            t.transmit(dst, code, p.nbits);
          }
        }
        return;
      }
    }
  }
}

optional<LidlAuriolData> LidlAuriolProtocol::decode(RemoteReceiveData src) {
  RemoteReceiveData s(src.get_raw_data(), 50, TOLERANCE_MODE_PERCENTAGE);

  uint8_t min_nbits = UINT8_MAX;

  for (const auto &t : TIMINGS) {
    for (const auto &p : t.protocols) {
      if (min_nbits > p.nbits)
        min_nbits = p.nbits;
    }
  }

  if (s.size() < min_nbits * 2 + 1) {
    return {};
  }

  for (const auto &t : TIMINGS) {
    uint8_t min_nbits = UINT8_MAX;
    uint8_t max_nbits = 0;

    for (const auto &p : t.protocols) {
      if (min_nbits > p.nbits)
        min_nbits = p.nbits;
      if (max_nbits < p.nbits)
        max_nbits = p.nbits;
    }

    s.reset();

    while (s.get_index() < s.size() - min_nbits * 2) {
      uint64_t code = 0;
      uint8_t nbits = 0;
      if (!t.receive(s, max_nbits, &code, &nbits)) {
        break;
      }

      if (nbits < min_nbits) {
        continue;
      }

      for (const auto &p : t.protocols) {
        if (nbits != p.nbits)
          continue;

        if (p.reversed) {
          uint64_t tmp = code;
          code = 0;
          for (int i = 0; i < nbits; i++) {
            code = (code << 1) | (tmp & 1);
            tmp >>= 1;
          }
        }

        ESP_LOGD(TAG, "%s code @%" PRIu32 " 0x%" PRIx64 " (%d)", p.model, s.get_index(), code, nbits);

        LidlAuriolData data;
        data.model = p.model;
        data.code = code;
        if (p.decode(data)) {
          // sanity check
          if (data.temperature >= -100 && data.temperature <= 100 && data.humidity <= 100) {
            return data;
          }
        }
      }
    }
  }

  return {};
}

void LidlAuriolProtocol::dump(const LidlAuriolData &data) {
  ESP_LOGI(TAG, "%s %" PRIx64 " id=%d b=%.0f ch=%d t=%.1f h=%d r=%.1f wd=%d ws=%.2f wg=%.2f", data.model.c_str(),
           data.code, data.id, data.battery_level, data.channel, data.temperature, data.humidity, data.rain,
           data.wind_direction_degrees, data.wind_speed, data.wind_gust);
}

}  // namespace remote_base
}  // namespace esphome
