#pragma once

#include "proto.h"
#include <cstdint>
#include <string>

namespace esphome {
namespace api {

class ProtoSize {
 public:
  // Core methods for Protocol Buffer serialization size calculation.
  // These methods calculate the exact byte counts needed for encoding various field types.

  static inline uint32_t varint(uint32_t value) {
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

  static inline uint32_t varint(uint64_t value) {
    // Handle common case of values fitting in uint32_t (vast majority of use cases)
    if (value <= UINT32_MAX) {
      return varint(static_cast<uint32_t>(value));
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

  static inline uint32_t varint(int32_t value) {
    // Negative values are sign-extended to 64 bits in protocol buffers,
    // which always results in a 10-byte varint for negative int32
    if (value < 0) {
      return 10;  // Negative int32 is always 10 bytes long
    }
    // For non-negative values, use the uint32_t implementation
    return varint(static_cast<uint32_t>(value));
  }

  static inline uint32_t varint(int64_t value) {
    // For int64_t, we convert to uint64_t and calculate the size
    // This works because the bit pattern determines the encoding size,
    // and we've handled negative int32 values as a special case above
    return varint(static_cast<uint64_t>(value));
  }

  static inline uint32_t field(uint32_t field_id, uint32_t type) {
    uint32_t tag = (field_id << 3) | (type & 0b111);
    return varint(tag);
  }

  /**
   * @brief Optimized size calculation for int32 fields with field ID
   *
   * This method is a specialized version for int32 fields that combines the field ID size
   * and the value size calculation. It's more efficient than separate calls to field_size
   * and varint_size, especially for negative values which have a fixed size.
   *
   * @param field_id_size Pre-calculated size of the field ID in bytes
   * @param value The int32 value to calculate size for
   * @return The total size in bytes (field ID + value)
   */
  static inline uint32_t int32_field_with_value(uint32_t field_id_size, int32_t value) {
    if (value < 0) {
      // Negative values are encoded as 10-byte varints in protobuf
      return field_id_size + 10;
    } else {
      // For non-negative values, use the standard varint size
      return field_id_size + varint(static_cast<uint32_t>(value));
    }
  }

  /**
   * @brief Directly adds the size of an int32 field to the total message size
   *
   * This version directly updates the total_size reference, which avoids an unnecessary
   * addition operation when the value is zero. This is more efficient for the common
   * case in Protocol Buffer messages where many fields have default values.
   *
   * @param total_size Reference to the total message size to update
   * @param field_id_size Pre-calculated size of the field ID in bytes
   * @param value The int32 value to calculate size for
   * @param force Whether to calculate size even if the value is zero
   */
  static inline void add_int32_field(uint32_t &total_size, uint32_t field_id_size, int32_t value, bool force = false) {
    // Skip calculation if value is zero and not forced
    if (value == 0 && !force) {
      return;  // No need to update total_size
    }

    // Calculate and directly add to total_size
    if (value < 0) {
      // Negative values are encoded as 10-byte varints in protobuf
      total_size += field_id_size + 10;
    } else {
      // For non-negative values, use the standard varint size
      total_size += field_id_size + varint(static_cast<uint32_t>(value));
    }
  }

  /**
   * @brief Directly adds the size of a uint32 field to the total message size
   *
   * This version directly updates the total_size reference, avoiding unnecessary
   * addition operations when the value is zero and not forced.
   *
   * @param total_size Reference to the total message size to update
   * @param field_id_size Pre-calculated size of the field ID in bytes
   * @param value The uint32 value to calculate size for
   * @param force Whether to calculate size even if the value is zero
   */
  static inline void add_uint32_field(uint32_t &total_size, uint32_t field_id_size, uint32_t value,
                                      bool force = false) {
    // Skip calculation if value is zero and not forced
    if (value == 0 && !force) {
      return;  // No need to update total_size
    }

    // Calculate and directly add to total_size
    total_size += field_id_size + varint(value);
  }

  /**
   * @brief Directly adds the size of a boolean field to the total message size
   *
   * This version directly updates the total_size reference, which avoids an unnecessary
   * addition operation when the value is false and not forced. Boolean fields use
   * a varint encoding and take exactly 1 byte when true.
   *
   * @param total_size Reference to the total message size to update
   * @param field_id_size Pre-calculated size of the field ID in bytes
   * @param value The boolean value to calculate size for
   * @param force Whether to calculate size even if the value is false
   */
  static inline void add_bool_field(uint32_t &total_size, uint32_t field_id_size, bool value, bool force = false) {
    // Skip calculation if value is false and not forced
    if (!value && !force) {
      return;  // No need to update total_size
    }

    // Boolean fields always use 1 byte when true
    total_size += field_id_size + 1;
  }

  /**
   * @brief Directly adds the size of a fixed32 or float field to the total message size
   *
   * Fixed32 fields always take exactly 4 bytes. This method directly updates the total_size
   * reference, avoiding unnecessary addition operations when the value is zero.
   *
   * @param total_size Reference to the total message size to update
   * @param field_id_size Pre-calculated size of the field ID in bytes
   * @param value The value to check if it's zero (for float, use 0.0f)
   * @param force Whether to calculate size even if the value is zero
   */
  static inline void add_fixed32_field(uint32_t &total_size, uint32_t field_id_size, bool is_nonzero,
                                       bool force = false) {
    // Skip calculation if value is zero and not forced
    if (!is_nonzero && !force) {
      return;  // No need to update total_size
    }

    // Fixed32 fields always take exactly 4 bytes
    total_size += field_id_size + 4;
  }

  /**
   * @brief Directly adds the size of a fixed64 or double field to the total message size
   *
   * Fixed64 fields always take exactly 8 bytes. This method directly updates the total_size
   * reference, avoiding unnecessary addition operations when the value is zero.
   *
   * @param total_size Reference to the total message size to update
   * @param field_id_size Pre-calculated size of the field ID in bytes
   * @param value The value to check if it's zero (for double, use 0.0)
   * @param force Whether to calculate size even if the value is zero
   */
  static inline void add_fixed64_field(uint32_t &total_size, uint32_t field_id_size, bool is_nonzero,
                                       bool force = false) {
    // Skip calculation if value is zero and not forced
    if (!is_nonzero && !force) {
      return;  // No need to update total_size
    }

    // Fixed64 fields always take exactly 8 bytes
    total_size += field_id_size + 8;
  }

  /**
   * @brief Directly adds the size of an enum field to the total message size
   *
   * Enum fields are encoded as uint32 varints. This method directly updates the total_size
   * reference, avoiding unnecessary addition operations when the value is zero.
   *
   * @param total_size Reference to the total message size to update
   * @param field_id_size Pre-calculated size of the field ID in bytes
   * @param value The enum value as an integer
   * @param force Whether to calculate size even if the value is zero
   */
  static inline void add_enum_field(uint32_t &total_size, uint32_t field_id_size, uint32_t value, bool force = false) {
    // Skip calculation if value is zero and not forced
    if (value == 0 && !force) {
      return;  // No need to update total_size
    }

    // Enums are encoded as uint32
    total_size += field_id_size + varint(value);
  }

  /**
   * @brief Directly adds the size of a string/bytes field to the total message size
   *
   * This version directly updates the total_size reference, which avoids an unnecessary
   * addition operation when the string is empty and not forced. This is more efficient
   * for the common case in Protocol Buffer messages.
   *
   * @param total_size Reference to the total message size to update
   * @param field_id_size Pre-calculated size of the field ID in bytes
   * @param str The string or bytes value to calculate size for
   * @param force Whether to calculate size even if the string is empty
   */
  static inline void add_string_field(uint32_t &total_size, uint32_t field_id_size, const std::string &str,
                                      bool force = false) {
    // Skip calculation if string is empty and not forced
    if (str.empty() && !force) {
      return;  // No need to update total_size
    }

    // Calculate and directly add to total_size
    const uint32_t str_size = static_cast<uint32_t>(str.size());
    total_size += field_id_size + varint(str_size) + str_size;
  }
};

}  // namespace api
}  // namespace esphome
