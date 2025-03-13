#pragma once

#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct WeatherStationData {
  uint16_t id{};
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
  bool receive_(RemoteReceiveData &src, std::vector<uint8_t> &code) const;
  void transmit_item_(RemoteTransmitData *dst, uint32_t high, uint32_t low) const;
  void transmit_(RemoteTransmitData *dst, const std::vector<uint8_t> &code) const;
};

template<typename... Ts> class WeatherStationAction : public RemoteTransmitterActionBase<Ts...> {
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

 protected:
  void get_data_(WeatherStationData &data, Ts... x) {
    data.id = this->id_.value(x...);
    data.battery_level = this->battery_level_.value(x...);
    data.channel = this->channel_.value(x...);
    data.temperature = this->temperature_.value(x...);
    data.humidity = this->humidity_.value(x...);
    data.rain = this->rain_.value(x...);
    data.wind_direction_degrees = this->wind_direction_degrees_.value(x...);
    data.wind_speed = this->wind_speed_.value(x...);
    data.wind_gust = this->wind_gust_.value(x...);
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

using WeatherStationDumper = RemoteReceiverDumper<WeatherStationProtocol>;

// WS2032

class WeatherStation2032Protocol : public WeatherStationProtocol {
 protected:
  void setup() override;
  bool transform(const std::vector<uint8_t> &code, WeatherStationData &data) const override;
  bool transform(const WeatherStationData &data, std::vector<uint8_t> &code) const override;
};

using WeatherStation2032BinarySensor = WeatherStationBinarySensor<WeatherStation2032Protocol>;
using WeatherStation2032Trigger = RemoteReceiverTrigger<WeatherStation2032Protocol>;

template<typename... Ts> class WeatherStation2032Action : public WeatherStationAction<Ts...> {
 public:
  void encode(RemoteTransmitData *dst, Ts... x) override {
    WeatherStationData data{};
    this->get_data_(data, x...);
    WeatherStation2032Protocol().encode(dst, data);
  }
};

// 4LD631

class WeatherStation4LD631Protocol : public WeatherStationProtocol {
 protected:
  void setup() override;
  bool transform(const std::vector<uint8_t> &code, WeatherStationData &data) const override;
  bool transform(const WeatherStationData &data, std::vector<uint8_t> &code) const override;
};

using WeatherStation4LD631BinarySensor = WeatherStationBinarySensor<WeatherStation4LD631Protocol>;
using WeatherStation4LD631Trigger = RemoteReceiverTrigger<WeatherStation4LD631Protocol>;

template<typename... Ts> class WeatherStation4LD631Action : public WeatherStationAction<Ts...> {
 public:
  void encode(RemoteTransmitData *dst, Ts... x) override {
    WeatherStationData data{};
    this->get_data_(data, x...);
    WeatherStation4LD631Protocol().encode(dst, data);
  }
};

// H10515

class WeatherStationH10515Protocol : public WeatherStationProtocol {
 protected:
  void setup() override;
  bool transform(const std::vector<uint8_t> &code, WeatherStationData &data) const override;
  bool transform(const WeatherStationData &data, std::vector<uint8_t> &code) const override;
};

using WeatherStationH10515BinarySensor = WeatherStationBinarySensor<WeatherStationH10515Protocol>;
using WeatherStationH10515Trigger = RemoteReceiverTrigger<WeatherStationH10515Protocol>;

template<typename... Ts> class WeatherStationH10515Action : public WeatherStationAction<Ts...> {
 public:
  void encode(RemoteTransmitData *dst, Ts... x) override {
    WeatherStationData data{};
    this->get_data_(data, x...);
    WeatherStationH10515Protocol().encode(dst, data);
  }
};

// L08037A

class WeatherStationL08037AProtocol : public WeatherStationProtocol {
 protected:
  void setup() override;
  bool transform(const std::vector<uint8_t> &code, WeatherStationData &data) const override;
  bool transform(const WeatherStationData &data, std::vector<uint8_t> &code) const override;
};

using WeatherStationL08037ABinarySensor = WeatherStationBinarySensor<WeatherStationL08037AProtocol>;
using WeatherStationL08037ATrigger = RemoteReceiverTrigger<WeatherStationL08037AProtocol>;

template<typename... Ts> class WeatherStationL08037AAction : public WeatherStationAction<Ts...> {
 public:
  void encode(RemoteTransmitData *dst, Ts... x) override {
    WeatherStationData data{};
    this->get_data_(data, x...);
    WeatherStationL08037AProtocol().encode(dst, data);
  }
};

// NEXUS

class WeatherStationNexusProtocol : public WeatherStationProtocol {
 protected:
  void setup() override;
  bool transform(const std::vector<uint8_t> &code, WeatherStationData &data) const override;
  bool transform(const WeatherStationData &data, std::vector<uint8_t> &code) const override;
};

using WeatherStationNexusBinarySensor = WeatherStationBinarySensor<WeatherStationNexusProtocol>;
using WeatherStationNexusTrigger = RemoteReceiverTrigger<WeatherStationNexusProtocol>;

template<typename... Ts> class WeatherStationNexusAction : public WeatherStationAction<Ts...> {
 public:
  void encode(RemoteTransmitData *dst, Ts... x) override {
    WeatherStationData data{};
    this->get_data_(data, x...);
    WeatherStationNexusProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
