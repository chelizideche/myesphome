#pragma once

#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct WeatherStationData {
  uint8_t id{};
  float battery_level{};
  uint8_t channel{};
  float temperature{};
  uint8_t humidity{};
  float rain{};
  float wind_direction_degrees{};
  float wind_speed{};
  float wind_gust{};
  // TODO: pressure
};

class WeatherStationProtocol : public RemoteProtocol<WeatherStationData> {
 public:
  void encode(RemoteTransmitData *dst, const WeatherStationData &data) override;
  optional<WeatherStationData> decode(RemoteReceiveData src) override;
  void dump(const WeatherStationData &data) override;

 protected:
  uint16_t sync_high_;
  uint16_t sync_low_;
  uint16_t zero_high_;
  uint16_t zero_low_;
  uint16_t one_high_;
  uint16_t one_low_;
  bool inverted_;
  uint8_t nbits_;
  uint8_t repeat_;
  bool reversed_;

  virtual void setup() = 0;
  virtual bool transform(const std::vector<uint8_t> &code, WeatherStationData &data) const = 0;
  virtual bool transform(const WeatherStationData &data, std::vector<uint8_t> &code) const = 0;

  bool receive_item_(RemoteReceiveData &src, uint32_t high, uint32_t low) const;
  bool receive_code_(RemoteReceiveData &src, std::vector<uint8_t> &code) const;
  void transmit_item_(RemoteTransmitData *dst, uint32_t high, uint32_t low) const;
  void transmit_code_(RemoteTransmitData *dst, const std::vector<uint8_t> &code) const;
};

template<typename P, typename... Ts> class WeatherStationAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, id)
  TEMPLATABLE_VALUE(float, battery_level)
  TEMPLATABLE_VALUE(uint8_t, channel)
  TEMPLATABLE_VALUE(float, temperature)
  TEMPLATABLE_VALUE(uint8_t, humidity)
  TEMPLATABLE_VALUE(float, rain)
  TEMPLATABLE_VALUE(float, wind_direction_degrees)
  TEMPLATABLE_VALUE(float, wind_speed)
  TEMPLATABLE_VALUE(float, wind_gust)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    WeatherStationData data{};
    data.id = this->id_.value(x...);
    data.battery_level = this->battery_level_.value(x...);
    data.channel = this->channel_.value(x...);
    data.temperature = this->temperature_.value(x...);
    data.humidity = this->humidity_.value(x...);
    data.rain = this->rain_.value(x...);
    data.wind_direction_degrees = this->wind_direction_degrees_.value(x...);
    data.wind_speed = this->wind_speed_.value(x...);
    data.wind_gust = this->wind_gust_.value(x...);
    P().encode(dst, data);
  }
};

template<typename T> class WeatherStationBinarySensor : public RemoteReceiverBinarySensorBase {
 protected:
  bool matches(RemoteReceiveData src) override {
    auto proto = T();
    auto res = proto.decode(src);
    return res.has_value();
  }
};

#define DECLARE_WEATHER_STATION_PROTOCOL_(prefix) \
  class prefix##Protocol : public WeatherStationProtocol { \
   protected: \
    void setup() override; \
    bool transform(const std::vector<uint8_t> &code, WeatherStationData &data) const override; \
    bool transform(const WeatherStationData &data, std::vector<uint8_t> &code) const override; \
  }; \
  using prefix##BinarySensor = WeatherStationBinarySensor<prefix##Protocol>; \
  using prefix##Trigger = RemoteReceiverTrigger<prefix##Protocol>; \
  using prefix##Dumper = RemoteReceiverDumper<prefix##Protocol>; \
  template<typename... Ts> class prefix##Action : public WeatherStationAction<prefix##Protocol, Ts...> {};
#define DECLARE_WEATHER_STATION_PROTOCOL(prefix) DECLARE_WEATHER_STATION_PROTOCOL_(prefix)

DECLARE_WEATHER_STATION_PROTOCOL(WeatherStation2032)
DECLARE_WEATHER_STATION_PROTOCOL(WeatherStation4LD)
DECLARE_WEATHER_STATION_PROTOCOL(WeatherStationBresser3CH)
DECLARE_WEATHER_STATION_PROTOCOL(WeatherStationEurochron)
DECLARE_WEATHER_STATION_PROTOCOL(WeatherStationH10515)
DECLARE_WEATHER_STATION_PROTOCOL(WeatherStationH13726)
DECLARE_WEATHER_STATION_PROTOCOL(WeatherStationL08037A)
DECLARE_WEATHER_STATION_PROTOCOL(WeatherStationNexus)

}  // namespace remote_base
}  // namespace esphome
