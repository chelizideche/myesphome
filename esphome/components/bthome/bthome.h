#pragma once

#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#include "esphome/core/component.h"

#ifdef USE_ESP32

namespace esphome {
namespace bthome {

uint32_t read_uint(size_t size, const uint8_t *data);
int32_t read_sint(size_t size, const uint8_t *data);

struct BoolValue {
  bool value;
  const uint8_t *next_ptr;
};

struct UInt32Value {
  uint32_t value;
  const uint8_t *next_ptr;
};

struct FloatValue {
  float value;
  const uint8_t *next_ptr;
};

class Publisher {
 public:
  Publisher(uint8_t oid): oid_(oid) {}
  virtual ~Publisher() {}
  uint8_t oid() const { return this->oid_; }
  virtual const uint8_t *publish(const uint8_t *data, size_t size) = 0;

 protected:
  uint8_t oid_;
};

class SensorPublisher: public Publisher {
 public:
  SensorPublisher(uint8_t oid, FloatValue (*read)(const uint8_t *data, size_t size), sensor::Sensor* sensor): Publisher(oid), sensor_(sensor), read_(read) {}
  virtual ~SensorPublisher() {}

  virtual const uint8_t *publish(const uint8_t *data, size_t size) {
    FloatValue v = this->read_(data, size);
    sensor_->publish_state(v.value);
    return v.next_ptr;
  }

 protected:
  sensor::Sensor *sensor_;
  FloatValue (*read_)(const uint8_t *data, size_t size);
};

class BinarySensorPublisher: public Publisher {
 public:
  BinarySensorPublisher(uint8_t oid, BoolValue (*read)(const uint8_t *data, size_t size), binary_sensor::BinarySensor* sensor): Publisher(oid), sensor_(sensor), read_(read) {}
  virtual ~BinarySensorPublisher() {}

  virtual const uint8_t *publish(const uint8_t *data, size_t size) {
    BoolValue v = this->read_(data, size);
    sensor_->publish_state(v.value);
    return v.next_ptr;
  }

 protected:
  binary_sensor::BinarySensor *sensor_;
  BoolValue (*read_)(const uint8_t *data, size_t size);
};

struct ScanResult {
  const uint8_t *value_ptr;
  size_t value_size;
  const uint8_t *next_ptr;
};

typedef ScanResult scan_func_t(const uint8_t *data, size_t size);

template <uint8_t oid, size_t size_bytes>
class OIDFixedSize {
 public:
  static ScanResult scan(const uint8_t *data, size_t size) {
    if (size < size_bytes) {
        return ScanResult {
          .value_ptr = nullptr,
          .value_size = 0,
          .next_ptr = data + size,
        };
    }
    return ScanResult {
      .value_ptr = data,
      .value_size = size_bytes,
      .next_ptr = data + size_bytes,
    };
  }

  static constexpr uint8_t oid_ = {oid};
  static constexpr size_t size_bytes_ = {size_bytes};
};

template <uint8_t oid>
class OIDBool: public OIDFixedSize <oid, 1> {
 public:
  static BoolValue read(const uint8_t *data, size_t size) {
    ScanResult sr = OIDBool::scan(data, size);
    if (sr.value_ptr == nullptr || sr.value_size == 0) {
      return BoolValue {
        .value = false,
        .next_ptr = sr.next_ptr,
      };
    }
    return BoolValue {
      .value = read_uint(sr.value_size, sr.value_ptr) != 0,
      .next_ptr = sr.next_ptr,
    };
  }

  BinarySensorPublisher *new_publisher(binary_sensor::BinarySensor *sensor) const {
    return new BinarySensorPublisher(oid, OIDBool::read, sensor);
  }
};

template <uint8_t oid, size_t size_bytes>
class OIDUInt: public OIDFixedSize <oid, size_bytes> {
 public:
  static UInt32Value read(const uint8_t *data, size_t size) {
    ScanResult sr = OIDUInt::scan(data, size);
    if (sr.value_ptr == nullptr || sr.value_size == 0) {
      return UInt32Value {
        .value = 0,
        .next_ptr = sr.next_ptr,
      };
    }
    return UInt32Value {
      .value = read_uint(sr.value_size, sr.value_ptr),
      .next_ptr = sr.next_ptr,
    };
  }
};

template <uint8_t oid, size_t size_bytes, int f_num=1, int f_denom=1>
class OIDSFixedPoint: public OIDFixedSize <oid, size_bytes> {
 public:
  static FloatValue read(const uint8_t *data, size_t size) {
    ScanResult sr = OIDSFixedPoint::scan(data, size);
    if (sr.value_ptr == nullptr || sr.value_size == 0) {
      return FloatValue {
        .value = 0.0,
        .next_ptr = sr.next_ptr,
      };
    }
    return FloatValue {
      .value = static_cast<float>(read_sint(sr.value_size, sr.value_ptr)) * f_num / f_denom,
      .next_ptr = sr.next_ptr,
    };
  }

  SensorPublisher *new_publisher(sensor::Sensor *sensor) const {
    return new SensorPublisher(oid, OIDSFixedPoint::read, sensor);
  }
  static constexpr int f_num_ = {f_num};
  static constexpr int f_denom_ = {f_denom};
};

template <uint8_t oid, size_t size_bytes, int f_num=1, int f_denom=1>
class OIDUFixedPoint: public OIDFixedSize <oid, size_bytes> {
 public:
  static FloatValue read(const uint8_t *data, size_t size) {
    ScanResult sr = OIDUFixedPoint::scan(data, size);
    if (sr.value_ptr == nullptr || sr.value_size == 0) {
      return FloatValue {
        .value = 0.0,
        .next_ptr = sr.next_ptr,
      };
    }
    return FloatValue {
      .value = static_cast<float>(read_uint(sr.value_size, sr.value_ptr)) * f_num / f_denom,
      .next_ptr = sr.next_ptr,
    };
  }

  SensorPublisher *new_publisher(sensor::Sensor *sensor) const {
    return new SensorPublisher(oid, OIDUFixedPoint::read, sensor);
  }

  static constexpr int f_num_ = {f_num};
  static constexpr int f_denom_ = {f_denom};
};

template <uint8_t oid>
class OIDUInt8: public OIDUInt <oid, 1> {
};

class BTHome : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { this->address_ = address; };
  void set_encryption_key(const std::string &encryption_key);

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

#ifdef USE_BINARY_SENSOR
  void set_window(binary_sensor::BinarySensor *window);
  void set_motion(binary_sensor::BinarySensor *motion);
#endif

#ifdef USE_SENSOR
  void set_angle(sensor::Sensor *angle);
  void set_illuminance(sensor::Sensor *illuminance);
  void set_battery_level(sensor::Sensor *battery_level);
  void set_temperature(sensor::Sensor *temperature);
  void set_humidity(sensor::Sensor *humidity);
#endif

 protected:
  const uint8_t *publish(uint8_t oid, const uint8_t *data, size_t size);
  void set_publisher(Publisher *publisher);

  uint64_t address_;
  bool encrypted_{false};
  uint8_t encryption_key_[16];

  int16_t last_pid_{-1}; // -1 indicates that no packets have been received yet.
  uint32_t last_counter_{0};

  std::vector <Publisher*> publishers_;

#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *window_{nullptr};
  binary_sensor::BinarySensor *motion_{nullptr};
#endif
#ifdef USE_SENSOR
  sensor::Sensor *angle_{nullptr};
  sensor::Sensor *illuminance_{nullptr};
  sensor::Sensor *battery_level_{nullptr};
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *humidity_{nullptr};
#endif
};

}  // namespace bthome
}  // namespace esphome

#endif
