#include "esphome/core/log.h"
#include "emontx_sensor.h"
namespace esphome {
namespace emontx {
static const char *const TAG = "emontx_sensor";
EmonTxSensor::EmonTxSensor(const char *tag) { this->tag = std::string(tag); }
void EmonTxSensor::publish_val(const std::string &val) {
  auto newval = parse_number<float>(val).value_or(0.0f);
  publish_state(newval);
}
void EmonTxSensor::dump_config() { LOG_SENSOR("  ", "EmonTx Sensor", this); }
}  // namespace emontx
}  // namespace esphome
