#pragma once

#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct LidlAuriolData {
  uint64_t code;
  uint8_t id;
  bool battery_level;
  uint8_t channel;
  float temperature;
  uint32_t rain;
  bool operator==(const LidlAuriolData &data) const { return code == data.code; }
};

class LidlAuriolProtocol : public RemoteProtocol<LidlAuriolData> {
 public:
  void encode(RemoteTransmitData *dst, const LidlAuriolData &data) override;
  optional<LidlAuriolData> decode(RemoteReceiveData src) override;
  void dump(const LidlAuriolData &data) override;
};

DECLARE_REMOTE_PROTOCOL(LidlAuriol)

template<typename... Ts> class LidlAuriolAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint8_t, id)
  TEMPLATABLE_VALUE(bool, battery_level)
  TEMPLATABLE_VALUE(uint8_t, channel)
  TEMPLATABLE_VALUE(float, temperature)
  TEMPLATABLE_VALUE(uint32_t, rain)
  void encode(RemoteTransmitData *dst, Ts... x) override {
    LidlAuriolData data{};
    data.id = this->id_.value(x...);
    data.battery_level = this->battery_level_.value(x...);
    data.channel = this->channel_.value(x...);
    data.temperature = this->temperature_.value(x...);
    data.rain = this->rain_.value(x...);
    LidlAuriolProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
