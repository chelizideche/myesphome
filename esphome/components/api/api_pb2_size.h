#pragma once

#include "proto.h"
#include <cstdint>
#include <string>

namespace esphome {
namespace api {

class ProtoSizeCalculator {
 public:
  // Only the varint_size and field_size methods are actually used
  // in the generated code. All other methods have been removed as they're unused.

  static uint32_t varint_size(uint32_t value) {
    // Optimized varint size calculation using leading zeros
    // Each 7 bits requires one byte in the varint encoding
    if (value < 128)
      return 1;  // 7 bits, common case for small values

    // For larger values, count bytes needed based on the position of the highest bit set
    if (value < 16384)
      return 2;  // 14 bits
    else if (value < 2097152)
      return 3;  // 21 bits
    else if (value < 268435456)
      return 4;  // 28 bits
    else
      return 5;  // 32 bits (maximum for uint32_t)
  }

  static uint32_t varint_size(uint64_t value) {
    // Handle common case of values fitting in uint32_t (vast majority of use cases)
    if (value <= UINT32_MAX) {
      return varint_size(static_cast<uint32_t>(value));
    }

    // For larger values, determine size based on highest bit position
    if (value < (1ULL << 35))
      return 5;  // 35 bits
    else if (value < (1ULL << 42))
      return 6;  // 42 bits
    else if (value < (1ULL << 49))
      return 7;  // 49 bits
    else if (value < (1ULL << 56))
      return 8;  // 56 bits
    else if (value < (1ULL << 63))
      return 9;  // 63 bits
    else
      return 10;  // 64 bits (maximum for uint64_t)
  }

  static uint32_t varint_size(int32_t value) {
    // Negative values are sign-extended to 64 bits in protocol buffers,
    // which always results in a 10-byte varint for negative int32
    if (value < 0) {
      return 10;  // Negative int32 is always 10 bytes long
    }
    // For non-negative values, use the uint32_t implementation
    return varint_size(static_cast<uint32_t>(value));
  }

  static uint32_t varint_size(int64_t value) {
    // For int64_t, we convert to uint64_t and calculate the size
    // This works because the bit pattern determines the encoding size,
    // and we've handled negative int32 values as a special case above
    return varint_size(static_cast<uint64_t>(value));
  }

  static uint32_t field_size(uint32_t field_id, uint32_t type) {
    uint32_t tag = (field_id << 3) | (type & 0b111);
    return varint_size(tag);
  }
};

}  // namespace api
}  // namespace esphome