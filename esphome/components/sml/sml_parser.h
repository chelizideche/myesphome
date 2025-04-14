#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "constants.h"
#include "span.h"

namespace esphome {
namespace sml {

using bytes = std::vector<uint8_t>;
using byte_span = Span<const uint8_t>;

struct ObisInfo {
  byte_span server_id;
  byte_span code;
  uint8_t unit;
  int8_t scaler;
  byte_span value;
  uint16_t value_type;
  std::string code_repr() const;
};

void for_each_obis_info(uint8_t const *begin, uint8_t const *end,
                        const std::function<void(const ObisInfo &)> &callback);

std::string bytes_repr(const byte_span &buffer);

uint64_t bytes_to_uint(const byte_span &buffer);

int64_t bytes_to_int(const byte_span &buffer);

std::string bytes_to_string(const byte_span &buffer);
}  // namespace sml
}  // namespace esphome
