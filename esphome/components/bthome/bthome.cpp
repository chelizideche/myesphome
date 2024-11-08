#include "bthome.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#include "mbedtls/ccm.h"

namespace esphome {
namespace bthome {

// 0x00    packet id   uint8 (1 byte)  0009    9
// 0x01    battery     uint8 (1 byte)  1   0161    97  %
// 0x02    temperature     sint16 (2 bytes)    0.01    02CA09  25.06   °C
// 0x03    humidity    uint16 (2 bytes)    0.01    03BF13  50.55   %
// 0x04    pressure    uint24 (3 bytes)    0.01    04138A01    1008.83     hPa
// 0x05    illuminance     uint24 (3 bytes)    0.01    05138A14    13460.67    lux
// 0x06    mass (kg)   uint16 (2 byte)     0.01    065E1F  80.3    kg
// 0x07    mass (lb)   uint16 (2 byte)     0.01    073E1D  74.86   lb
// 0x08    dewpoint    sint16 (2 bytes)    0.01    08CA06  17.38   °C
// 0x09    count   uint (1 bytes)  1   0960    96
// 0x0A    energy  uint24 (3 bytes)    0.001   0A138A14    1346.067    kWh
// 0x0B    power   uint24 (3 bytes)    0.01    0B021B00    69.14   W
// 0x0C    voltage     uint16 (2 bytes)    0.001   0C020C  3.074   V
// 0x0D    pm2.5   uint16 (2 bytes)    1   0D120C  3090    ug/m3
// 0x0E    pm10    uint16 (2 bytes)    1   0E021C  7170    ug/m3
// 0x0F    generic boolean     uint8 (1 byte)  0F01    0 (False = Off) 1 (True = On)
// 0x10    power   uint8 (1 byte)  1001    0 (False = Off) 1 (True = On)
// 0x11    opening     uint8 (1 byte)  1100    0 (False = Closed) 1 (True = Open)
// 0x12    co2     uint16 (2 bytes)    1   12E204  1250    ppm
// 0x13    tvoc    uint16 (2 bytes)    1   133301  307     ug/m3
// 0x14    moisture    uint16 (2 bytes)    0.01    14020C  30.74   %
// 0x15    battery     uint8 (1 byte)  1501    0 (False = Normal) 1 (True = Low)
// 0x16    battery charging    uint8 (1 byte)  1601    0 (False = Not Charging) 1 (True = Charging)
// 0x17    carbon monoxide     uint8 (1 byte)  1700    0 (False = Not detected) 1 (True = Detected)
// 0x18    cold    uint8 (1 byte)  1801    0 (False = Normal) 1 (True = Cold)
// 0x19    connectivity    uint8 (1 byte)  1900    0 (False = Disconnected) 1 (True = Connected)
// 0x1A    door    uint8 (1 byte)  1A00    0 (False = Closed) 1 (True = Open)
// 0x1B    garage door     uint8 (1 byte)  1B01    0 (False = Closed) 1 (True = Open)
// 0x1C    gas     uint8 (1 byte)  1C01    0 (False = Clear) 1 (True = Detected)
// 0x1D    heat    uint8 (1 byte)  1D00    0 (False = Normal) 1 (True = Hot)
// 0x1E    light   uint8 (1 byte)  1E01    0 (False = No light) 1 (True = Light detected)
// 0x1F    lock    uint8 (1 byte)  1F01    0 (False = Locked) 1 (True = Unlocked)
// 0x20    moisture    uint8 (1 byte)  2001    0 (False = Dry) 1 (True = Wet)
// 0x21    motion  uint8 (1 byte)  2100    0 (False = Clear) 1 (True = Detected)
// 0x22    moving  uint8 (1 byte)  2201    0 (False = Not moving) 1 (True = Moving)
// 0x23    occupancy   uint8 (1 byte)  2301    0 (False = Clear) 1 (True = Detected)
// 0x24    plug    uint8 (1 byte)  2400    0 (False = Unplugged) 1 (True = Plugged in)
// 0x25    presence    uint8 (1 byte)  2500    0 (False = Away) 1 (True = Home)
// 0x26    problem     uint8 (1 byte)  2601    0 (False = OK) 1 (True = Problem)
// 0x27    running     uint8 (1 byte)  2701    0 (False = Not Running) 1 (True = Running)
// 0x28    safety  uint8 (1 byte)  2800    0 (False = Unsafe) 1 (True = Safe)
// 0x29    smoke   uint8 (1 byte)  2901    0 (False = Clear) 1 (True = Detected)
// 0x2A    sound   uint8 (1 byte)  2A00    0 (False = Clear) 1 (True = Detected)
// 0x2B    tamper  uint8 (1 byte)  2B00    0 (False = Off) 1 (True = On)
// 0x2C    vibration   uint8 (1 byte)  2C01    0 (False = Clear) 1 (True = Detected)
// 0x2D    window  uint8 (1 byte)  2D01    0 (False = Closed) 1 (True = Open)
// 0x2E    humidity    uint8 (1 byte)  1   2E23    35  %
// 0x2F    moisture    uint8 (1 byte)  1   2F23    35  %
// 0x3A    button  0x00    None        3A00    0x01    press       3A01    press 0x02    double_press        3A02    double_press 0x03    triple_press        3A03    triple_press 0x04    long_press      3A04    long_press 0x05    long_double_press       3A05    long_double_press 0x06    long_triple_press       3A06    long_triple_press 0x80    hold_press      3A80    hold_press
// 0x3C    dimmer  0x00    None        3C0000  0x01    rotate left     # steps     3C0103  rotate left 3 steps 0x02    rotate right    # steps     3C020A  rotate right 10 steps
// 0x3D    count   uint (2 bytes)  1   3D0960  24585
// 0x3E    count   uint (4 bytes)  1   3E2A2C0960  1611213866
// 0x3F    rotation    sint16 (2 bytes)    0.1     3F020C  307.4   °
// 0x40    distance (mm)   uint16 (2 bytes)    1   400C00  12  mm
// 0x41    distance (m)    uint16 (2 bytes)    0.1     414E00  7.8     m
// 0x42    duration    uint24 (3 bytes)    0.001   424E3400    13.390  s
// 0x43    current     uint16 (2 bytes)    0.001   434E34  13.39   A
// 0x44    speed   uint16 (2 bytes)    0.01    444E34  133.90  m/s
// 0x45    temperature     sint16 (2 bytes)    0.1     451101  27.3    °C
// 0x46    UV index    uint8 (1 byte)  0.1     4632    5.0
// 0x47    volume  uint16 (2 bytes)    0.1     478756  2215.1  L
// 0x48    volume  uint16 (2 bytes)    1   48DC87  34780   mL
// 0x49    volume Flow Rate    uint16 (2 bytes)    0.001   49DC87  34.780  m3/hr
// 0x4A    voltage     uint16 (2 bytes)    0.1     4A020C  307.4   V
// 0x4B    gas     uint24 (3 bytes)    0.001   4B138A14    1346.067    m3
// 0x4C    gas     uint32 (4 bytes)    0.001   4C41018A01  25821.505   m3
// 0x4D    energy  uint32 (4 bytes)    0.001   4d12138a14  344593.170  kWh
// 0x4E    volume  uint32 (4 bytes)    0.001   4E87562A01  19551.879   L
// 0x4F    water   uint32 (4 bytes)    0.001   4F87562A01  19551.879
// 0x50    timestamp   uint48 (4 bytes)    -   505d396164  see below
// 0x51    acceleration    uint16 (2 bytes)    0.001   518756  22.151  m/s²
// 0x52    gyroscope   uint16 (2 bytes)    0.001   528756  22.151  °/s
// 0x53    text    see below   -   530C48656C6C6F 20576F726C6421   Hello World!
// 0x54    raw     see below   -   540C48656C6C6F 20576F726C6421   48656c6c6f20 576f726c6421
// 0x55    volume storage  uint32 (4 bytes)    0.001   5587562A01  19551.879   L
// 0xF0    device type id  uint16 (2 bytes)    F00100  1
// 0xF1    firmware version    uint32 (4 bytes)    F100010204  4.2.1.0
// 0xF2    firmware version    uint24 (3 bytes)    F2000106    6.1.0

static const char *const TAG = "bthome";

const OIDUInt8<0x00> oid_pid;
const OIDUFixedPoint<0x01, 1> oid_battery_percent;
const OIDSFixedPoint<0x02, 2, 1, 100> oid_temperature_celsius_x100;
const OIDUFixedPoint<0x03, 2, 1, 100> oid_humidity_percent_x100;
const OIDUFixedPoint<0x04, 3, 1, 100> oid_pressure_hpa_x100;
const OIDUFixedPoint<0x05, 3, 1, 100> oid_illuminance_lux_x100;
const OIDUFixedPoint<0x06, 2, 1, 100> oid_mass_kg_x100;
const OIDUFixedPoint<0x07, 2, 45359237, 100000000> oid_mass_lb_x100; // Converts to kg.
const OIDSFixedPoint<0x08, 2> oid_dewpoint_celsius_x100;
const OIDUFixedPoint<0x09, 1> oid_counter8;
const OIDUFixedPoint<0x0a, 3, 1, 1000> oid_energy_kwh_x1000;
const OIDUFixedPoint<0x0b, 3, 1, 100> oid_power_w_x100;
const OIDUFixedPoint<0x0c, 2, 1, 1000> oid_voltage_v_x1000;
const OIDUFixedPoint<0x0d, 2> oid_pm2_5_ug_m3;
const OIDUFixedPoint<0x0e, 2> oid_pm10_ug_m3;
const OIDBool<0x0f> oid_bool;
const OIDBool<0x10> oid_power;
const OIDBool<0x11> oid_opening;
const OIDUFixedPoint<0x12, 2> oid_co2_concentration_ppm;
const OIDUFixedPoint<0x13, 2> oid_tvoc_ug_m3;
const OIDUFixedPoint<0x14, 2, 1, 100> oid_moisture_percent_x100;
const OIDBool<0x15> oid_battery;
const OIDBool<0x16> oid_battery_charging;
const OIDBool<0x17> oid_carbon_monoxide;
const OIDBool<0x18> oid_cold;
const OIDBool<0x19> oid_connectivity;
const OIDBool<0x1a> oid_door;
const OIDBool<0x1b> oid_garage_door;
const OIDBool<0x1c> oid_gas;
const OIDBool<0x1d> oid_heat;
const OIDBool<0x1e> oid_light;
const OIDBool<0x1f> oid_lock;
const OIDBool<0x20> oid_moisture;
const OIDBool<0x21> oid_motion;
const OIDBool<0x22> oid_moving;
const OIDBool<0x23> oid_occupancy;
const OIDBool<0x24> oid_plug;
const OIDBool<0x25> oid_presence;
const OIDBool<0x26> oid_problem;
const OIDBool<0x27> oid_running;
const OIDBool<0x28> oid_safety;
const OIDBool<0x29> oid_smoke;
const OIDBool<0x2a> oid_sound;
const OIDBool<0x2b> oid_tamper;
const OIDBool<0x2c> oid_vibration;
const OIDBool<0x2d> oid_window;
const OIDUFixedPoint<0x2e, 1> oid_humidity_percent;
const OIDUFixedPoint<0x2f, 1> oid_moisture_percent;
// 0x30 - 0x39
const OIDUFixedPoint<0x3a, 1> oid_button_event;
// 0x3b
// 0x3C    dimmer  0x00    None        3C0000  0x01    rotate left     # steps     3C0103  rotate left 3 steps 0x02    rotate right    # steps     3C020A  rotate right 10 steps
const OIDUFixedPoint<0x3d, 2> oid_counter16;
const OIDUFixedPoint<0x3e, 4> oid_counter32;
const OIDUFixedPoint<0x3f, 2, 1, 10> oid_angle_degrees_x10;
// 0x40    distance (mm)   uint16 (2 bytes)    1   400C00  12  mm
// 0x41    distance (m)    uint16 (2 bytes)    0.1     414E00  7.8     m
// 0x42    duration    uint24 (3 bytes)    0.001   424E3400    13.390  s
// 0x43    current     uint16 (2 bytes)    0.001   434E34  13.39   A
// 0x44    speed   uint16 (2 bytes)    0.01    444E34  133.90  m/s
const OIDSFixedPoint<0x45, 2, 1, 10> oid_temperature_celsius_x10;
// 0x46    UV index    uint8 (1 byte)  0.1     4632    5.0
// 0x47    volume  uint16 (2 bytes)    0.1     478756  2215.1  L
// 0x48    volume  uint16 (2 bytes)    1   48DC87  34780   mL
// 0x49    volume Flow Rate    uint16 (2 bytes)    0.001   49DC87  34.780  m3/hr
// 0x4A    voltage     uint16 (2 bytes)    0.1     4A020C  307.4   V
// 0x4B    gas     uint24 (3 bytes)    0.001   4B138A14    1346.067    m3
// 0x4C    gas     uint32 (4 bytes)    0.001   4C41018A01  25821.505   m3
// 0x4D    energy  uint32 (4 bytes)    0.001   4d12138a14  344593.170  kWh
// 0x4E    volume  uint32 (4 bytes)    0.001   4E87562A01  19551.879   L
// 0x4F    water   uint32 (4 bytes)    0.001   4F87562A01  19551.879
// 0x50    timestamp   uint48 (4 bytes)    -   505d396164  see below
// 0x51    acceleration    uint16 (2 bytes)    0.001   518756  22.151  m/s²
// 0x52    gyroscope   uint16 (2 bytes)    0.001   528756  22.151  °/s
// 0x53    text    see below   -   530C48656C6C6F 20576F726C6421   Hello World!
// 0x54    raw     see below   -   540C48656C6C6F 20576F726C6421   48656c6c6f20 576f726c6421
// 0x55    volume storage  uint32 (4 bytes)    0.001   5587562A01  19551.879   L
// 0x56 - 0xef
// 0xF0    device type id  uint16 (2 bytes)    F00100  1
// 0xF1    firmware version    uint32 (4 bytes)    F100010204  4.2.1.0
// 0xF2    firmware version    uint24 (3 bytes)    F2000106    6.1.0

#define OID_ENTRY(oid) [oid.oid_] = oid.scan

static scan_func_t* oids[256] = {
  OID_ENTRY(oid_pid),
  OID_ENTRY(oid_battery_percent),
  OID_ENTRY(oid_temperature_celsius_x100),
  OID_ENTRY(oid_humidity_percent_x100),
  OID_ENTRY(oid_pressure_hpa_x100),
  OID_ENTRY(oid_illuminance_lux_x100),
  OID_ENTRY(oid_mass_kg_x100),
  OID_ENTRY(oid_mass_lb_x100),
  OID_ENTRY(oid_dewpoint_celsius_x100),
  OID_ENTRY(oid_counter8),
  OID_ENTRY(oid_energy_kwh_x1000),
  OID_ENTRY(oid_power_w_x100),
  OID_ENTRY(oid_voltage_v_x1000),
  OID_ENTRY(oid_pm2_5_ug_m3),
  OID_ENTRY(oid_pm10_ug_m3),
  OID_ENTRY(oid_bool),
  OID_ENTRY(oid_power),
  OID_ENTRY(oid_opening),
  OID_ENTRY(oid_co2_concentration_ppm),
  OID_ENTRY(oid_tvoc_ug_m3),
  OID_ENTRY(oid_moisture_percent_x100),
  OID_ENTRY(oid_battery),
  OID_ENTRY(oid_battery_charging),
  OID_ENTRY(oid_carbon_monoxide),
  OID_ENTRY(oid_cold),
  OID_ENTRY(oid_connectivity),
  OID_ENTRY(oid_door),
  OID_ENTRY(oid_garage_door),
  OID_ENTRY(oid_gas),
  OID_ENTRY(oid_heat),
  OID_ENTRY(oid_light),
  OID_ENTRY(oid_lock),
  OID_ENTRY(oid_moisture),
  OID_ENTRY(oid_motion),
  OID_ENTRY(oid_moving),
  OID_ENTRY(oid_occupancy),
  OID_ENTRY(oid_plug),
  OID_ENTRY(oid_presence),
  OID_ENTRY(oid_problem),
  OID_ENTRY(oid_running),
  OID_ENTRY(oid_safety),
  OID_ENTRY(oid_smoke),
  OID_ENTRY(oid_sound),
  OID_ENTRY(oid_tamper),
  OID_ENTRY(oid_vibration),
  OID_ENTRY(oid_window),
  OID_ENTRY(oid_humidity_percent),
  OID_ENTRY(oid_moisture_percent),
  [0x30] = nullptr,
  [0x31] = nullptr,
  [0x32] = nullptr,
  [0x33] = nullptr,
  [0x34] = nullptr,
  [0x35] = nullptr,
  [0x36] = nullptr,
  [0x37] = nullptr,
  [0x38] = nullptr,
  [0x39] = nullptr,
  OID_ENTRY(oid_button_event),
  [0x3b] = nullptr,
  [0x3c] = nullptr,
  OID_ENTRY(oid_counter16),
  OID_ENTRY(oid_counter32),
  OID_ENTRY(oid_angle_degrees_x10),
  [0x40] = nullptr,
  [0x41] = nullptr,
  [0x42] = nullptr,
  [0x43] = nullptr,
  [0x44] = nullptr,
  OID_ENTRY(oid_temperature_celsius_x10),
  [0x46] = nullptr,
  [0x47] = nullptr,
  [0x48] = nullptr,
  [0x49] = nullptr,
};

uint32_t read_uint(size_t size, const uint8_t *data) {
  if (size < 1 || size > 4) {
    ESP_LOGE(TAG, "read_uint() called with invalid size %zu, must be in range [1, 4]", size);
    return 0;
  }
  uint32_t val = 0;
  int shift = 0;
  for (int i=0; i<size; i++, shift+=8) {
    val |= data[i] << shift;
  }
  return val;
}

int32_t read_sint(size_t size, const uint8_t *data) {
  if (size < 1 || size > 4) {
    ESP_LOGE(TAG, "read_sint() called with invalid size %zu, must be in range [1, 4]", size);
    return 0;
  }
  bool positive = true;
  if (data[size-1] > 127) {
    positive = false;
  }

  int32_t val = 0;
  int shift = (size-1)*8;
  for (int i=size-1; i>=0; i--, shift-=8) {
    if (positive) {
      val |= data[i] << shift;
    } else {
      val |= (0xff - data[i]) << shift;
    }
  }
  if (!positive) {
    val = -val - 1;
  }
  return val;
}

// BTHome Device Information
// From https://bthome.io/format/
//
// The first byte after the UUID is the BTHome device info byte, which has
// several bits indicating the capabilities of the device.
//
//     bit 0: “Encryption flag”
//         The Encryption flag is telling the receiver wether the device is
//         sending non-encrypted data (bit 0 = 0) or encrypted data (bit 0 = 1).
//     bit 1: “Reserved for future use”
//     bit 2: “Trigger based device flag”
//         The trigger based device flag is telling the receiver that it should
//         expect that the device is sending BLE advertisements at a regular
//         interval (bit 2 = 0) or at an irregular interval (bit 2 = 1), e.g.
//         only when someone pushes a button. This can be useful information
//         for a receiver, e.g. to prevent the device from going to
//         unavailable.
//     bit 3-4: “Reserved for future use”
//     bit 5-7: “BTHome Version”
//         This represents the BTHome verion. Currently only BTHome version 1
//         or 2 are allowed, where 2 is the latest version (bit 5-7 = 010).
//
struct device_information {
  bool encryption;
  bool trigger_based;
  uint8_t bthome_version;

  device_information(uint8_t v) {
    encryption = v & 0b000000001;
    trigger_based = (v & 0b00000100) >> 2;
    bthome_version = (v & 0b11100000) >> 5;
  }
};

//
// data is a full data received from device. Must be > 9 bytes log.
// mac_address has to be 6 bytes long.
// cleartext has to be at least data_len - 9 bytes long.
int decrypt(const uint8_t *data, ssize_t data_len, const uint8_t *mac_address, uint16_t uuid, const uint8_t *key, ssize_t keybits, uint8_t *cleartext) {
  mbedtls_ccm_context ctx;
  mbedtls_ccm_init(&ctx);
  int ret = mbedtls_ccm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, keybits);
  if (ret) {
    ESP_LOGE(TAG, "mbedtls_ccm_setkey() failed: %04x", ret);
    mbedtls_ccm_free(&ctx);
    return ret;
  }

  // Nonce:
  //  MAC address 6 bytes
  //  UUID 2 bytes
  //  Device data 1 byte
  //  Counter 4 bytes
  uint8_t nonce[] = {
    [0x00] = mac_address[0x00],
    [0x01] = mac_address[0x01],
    [0x02] = mac_address[0x02],
    [0x03] = mac_address[0x03],
    [0x04] = mac_address[0x04],
    [0x05] = mac_address[0x05],
    [0x06] = (uint8_t) (uuid & 0xff),
    [0x07] = (uint8_t) ((uuid >> 8) & 0xff),
    [0x08] = data[0x00],
    [0x09] = data[data_len-8],
    [0x0A] = data[data_len-7],
    [0x0B] = data[data_len-6],
    [0x0C] = data[data_len-5],
  };

  uint8_t tag[] = {
    [0x00] = data[data_len-4],
    [0x01] = data[data_len-3],
    [0x02] = data[data_len-2],
    [0x03] = data[data_len-1],
  };

  ret = mbedtls_ccm_auth_decrypt(&ctx, data_len-9, nonce, sizeof(nonce)/sizeof(nonce[0]), NULL, 0, data+1, cleartext, tag, sizeof(tag)/sizeof(tag[0]));
  if (ret != 0) {
    ESP_LOGE(TAG, "mbedtls_ccm_auth_decrypt() failed: %04x", ret);
    mbedtls_ccm_free(&ctx);
    return ret;
  }

  mbedtls_ccm_free(&ctx);
  return 0;
}

void BTHome::dump_config() {
  ESP_LOGCONFIG(TAG, "BTHome");
  if (this->encrypted_) {
    // ESP_LOGCONFIG(TAG, "  Encryption key: %s", format_hex_pretty(this->encryption_key_, 16).c_str());
    ESP_LOGCONFIG(TAG, "  Encryption key set");
  } else {
    ESP_LOGCONFIG(TAG, "  Encryption key not set");
  }
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "window", this->window_);
  LOG_BINARY_SENSOR("  ", "motion", this->motion_);
#endif
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "illuminance", this->illuminance_);
  LOG_SENSOR("  ", "battery Level", this->battery_level_);
  LOG_SENSOR("  ", "temperature", this->temperature_);
  LOG_SENSOR("  ", "humidity", this->humidity_);
#endif
}

bool BTHome::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (device.address_uint64() != address_) {
    ESP_LOGVV(TAG, "parse_device(): unknown MAC address.");
    return false;
  }
  ESP_LOGVV(TAG, "parse_device(): MAC address %s found.", device.address_str().c_str());

  bool success = false;
  for (auto &service_data : device.get_service_datas()) {
    const auto BTHomeServiceDataUUID = 0xfcd2;
    if (service_data.uuid != esp32_ble::ESPBTUUID::from_uint16(BTHomeServiceDataUUID)) {
      continue;
    }
    if (service_data.data.size() < 1) {
      ESP_LOGW(TAG, "BTHome service data (UUID %#04x) without data (size %zd)", BTHomeServiceDataUUID, service_data.data.size());
      continue;
    }
    device_information di(service_data.data[0]);
    if (di.bthome_version != 2) {
      ESP_LOGW(TAG, "BTHome version %d is not supported (only version 2 is supported)", di.bthome_version);
      continue;
    }
    const uint8_t *data;
    size_t data_len;
    uint8_t cleartext[service_data.data.size()-9];
    if (di.encryption) {
      if (!this->encrypted_) {
        ESP_LOGW(TAG, "Encrypted packet but encryption key is not set");
        continue;
      }
      uint32_t counter =
        service_data.data[service_data.data.size()-8]
        + ((uint32_t)(service_data.data[service_data.data.size()-7]) << 8)
        + ((uint32_t)(service_data.data[service_data.data.size()-6]) << 16)
        + ((uint32_t)(service_data.data[service_data.data.size()-5]) << 24);
      if (this->last_pid_ != -1 && counter <= this->last_counter_) {
        ESP_LOGV(TAG, "Packet counter already seen, skipping");
        continue;
      }
      this->last_counter_ = counter;
      const auto keybits = sizeof(this->encryption_key_) / sizeof(this->encryption_key_[0]) * 8;
      int ret = decrypt(service_data.data.data(), service_data.data.size(), device.address(), BTHomeServiceDataUUID, this->encryption_key_, keybits, cleartext);
      if (ret) {
        ESP_LOGE(TAG, "Could not decrypt packet, error code: %04x", ret);
        continue;
      }
      data = cleartext;
      data_len = sizeof(cleartext) / sizeof(cleartext[0]);
    } else {
      if (this->encrypted_) {
        ESP_LOGW(TAG, "Ignoring unencrypted packet when encryption key is set");
        continue;
      }
      data = service_data.data.data() + 1;
      data_len = service_data.data.size() - 1;
    }
    for (const uint8_t *p = data; p < data + data_len;) {
      auto oid = *p++;
      ESP_LOGVV(TAG, "OID %#02x", oid);

      if (oid == oid_pid.oid_) {
        UInt32Value v = oid_pid.read(p, data + data_len - p);
        if (v.value == this->last_pid_) {
          ESP_LOGV(TAG, "Packet ID %d already seen, skipping", v.value);
          break;
        }
        this->last_pid_ = v.value;
        p = v.next_ptr;
        continue;
      }

      const uint8_t *next_ptr = this->publish(oid, p, data + data_len - p);
      if (next_ptr != nullptr) {
        p = next_ptr;
        continue;
      }

      auto scan = oids[oid];
      if (scan == NULL) {
        ESP_LOGW(TAG, "Unknown OID %#02x - parsing aborted", oid);
        break;
      }
      ESP_LOGD(TAG, "Skipping OID %#02x", oid);
      ScanResult sr = scan(p, data + data_len - p);
      p = sr.next_ptr;
    }
    success = true;
  }

  return success;
}

const uint8_t *BTHome::publish(uint8_t oid, const uint8_t *data, size_t size) {
  for (auto pub : this->publishers_) {
    if (pub->oid() == oid) {
      return pub->publish(data, size);
    }
  }
  return nullptr;
}

void BTHome::set_publisher(Publisher *publisher) {
  for (int i = 0; i < this->publishers_.size(); i++) {
    if (this->publishers_[i]->oid() == publisher->oid()) {
      auto old = this->publishers_[i];
      this->publishers_[i] = publisher;
      delete old;
      return;
    }
  }
  this->publishers_.push_back(publisher);
}

void BTHome::set_encryption_key(const std::string &encryption_key) {
  memset(this->encryption_key_, 0, 16);
  if (encryption_key.size() != 32) {
    return;
  }
  char temp[3] = {0};
  for (int i = 0; i < 16; i++) {
    strncpy(temp, &(encryption_key.c_str()[i * 2]), 2);
    this->encryption_key_[i] = std::strtoul(temp, nullptr, 16);
  }
  this->encrypted_ = true;
}

#ifdef USE_BINARY_SENSOR
void BTHome::set_window(binary_sensor::BinarySensor *window) {
  this->window_ = window;
  this->set_publisher(oid_window.new_publisher(window));
}
void BTHome::set_motion(binary_sensor::BinarySensor *motion) {
  this->motion_ = motion;
  this->set_publisher(oid_motion.new_publisher(motion));
}
#endif

#ifdef USE_SENSOR
void BTHome::set_angle(sensor::Sensor *angle) {
  this->angle_ = angle;
  this->set_publisher(oid_angle_degrees_x10.new_publisher(angle));
}
void BTHome::set_illuminance(sensor::Sensor *illuminance) {
  this->illuminance_ = illuminance;
  this->set_publisher(oid_illuminance_lux_x100.new_publisher(illuminance));
}
void BTHome::set_battery_level(sensor::Sensor *battery_level) {
  this->battery_level_ = battery_level;
  this->set_publisher(oid_battery_percent.new_publisher(battery_level));
}
void BTHome::set_humidity(sensor::Sensor *humidity) {
  this->humidity_ = humidity;
  this->set_publisher(oid_humidity_percent.new_publisher(humidity));
}
//void BTHome::set_temperature(sensor::Sensor *temperature) {
//  this->temperature_ = temperature;
//  this->set_publisher(oid_temperature_celsius_x100.new_publisher(temperature));
//}
void BTHome::set_temperature(sensor::Sensor *temperature) {
  this->temperature_ = temperature;
  this->set_publisher(oid_temperature_celsius_x10.new_publisher(temperature));
}
#endif

}  // namespace bthome
}  // namespace esphome

#endif
