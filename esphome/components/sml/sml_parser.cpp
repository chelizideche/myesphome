#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "constants.h"
#include "sml_parser.h"

namespace esphome {
namespace sml {

static const char *const TAG = "sml";

struct NodeTypeLength {
  uint16_t type;
  uint16_t length;
};

template<typename T> static T to_int(const uint8_t *ptr, const uint8_t *end) {
  static_assert(std::is_integral<T>::value, "T must be an integer type.");
  T val = 0;
  if (ptr != end) {
    val = static_cast<typename std::conditional<std::is_signed<T>::value, int8_t, uint8_t>::type>(*ptr++);
    while (ptr != end) {
      val = (val << 8) + *ptr++;
    }
  }
  return val;
}

class Parser {
 public:
  Parser(const uint8_t *ptr, const uint8_t *end) : ptr_(ptr), end_(end), error_(false) {}

  operator bool() const { return !this->error_ && this->ptr_ != this->end_; }

  bool error() const { return this->error_; }

  NodeTypeLength read_type_length() {
    if (this->error_) {
      return {SML_UNDEFINED, 0};
    }

    if (this->ptr_ == this->end_) {
      ESP_LOGW(TAG, "unexpected end of buffer while waiting for a node");
      this->error_ = true;
      return {SML_UNDEFINED, 0};
    }

    const auto *begin = this->ptr_;
    uint16_t length = *this->ptr_ & 0x0f;
    uint16_t type = (*this->ptr_ >> 4) & 0x07;
    bool has_extra_type_length_byte = (*this->ptr_ & 0x80) != 0;
    if (has_extra_type_length_byte) {
      if (++this->ptr_ == this->end_) {
        ESP_LOGW(TAG, "unexpected end of buffer while waiting for extra type-length byte");
        this->error_ = true;
        return {SML_UNDEFINED, 0};
      }
      length = (length << 4) + (*this->ptr_ & 0x0f);

      // Technically, this is not enough, the standard allows for more than two length fields.
      // However I don't think it will ever happen.
    }
    ++this->ptr_;

    if (type != SML_LIST) {
      if (this->end_ - begin < length) {
        ESP_LOGW(TAG, "unexpected end of buffer while waiting for node value");
        this->error_ = true;
        return {SML_UNDEFINED, 0};
      }
      length -= this->ptr_ - begin;
    }

    return {type, length};
  }

  uint16_t read_list_length(const char *name) {
    auto tl = this->read_type_length();
    if (this->error_) {
      return 0;
    }
    if (tl.type != SML_LIST) {
      ESP_LOGW(TAG, "unexpected node type %u when expecting list %s", tl.type, name);
      this->error_ = true;
      return 0;
    }
    return tl.length;
  }

  bool skip_nodes(unsigned n) {
    while (n-- > 0) {
      auto tl = this->read_type_length();
      if (this->error_) {
        return false;
      }
      if (tl.type == SML_LIST) {
        n += tl.length;
      } else {
        this->ptr_ += tl.length;
      }
    };
    return !this->error_;
  }

  bool skip_until_next_message() {
    while (this->ptr_ != this->end_) {
      if (*this->ptr_ == 0) {
        ++this->ptr_;
        break;
      }
      auto tl = this->read_type_length();
      if (this->error_) {
        return false;
      } else if (tl.type != SML_LIST) {
        this->ptr_ += tl.length;
      }
    }
    // Some messages are padded with zeros
    while (this->ptr_ != this->end_ && *this->ptr_ == 0) {
      ++this->ptr_;
    }
    return !this->error_;
  }

  template<typename T> bool read_int(T &result, const char *name) {
    auto tl = this->read_type_length();
    if (this->error_) {
      return false;
    }
    if (tl.type == SML_OCTET && tl.length == 0) {
      // Missing optional field (use default value 0)
      result = 0;
      return true;
    }
    if (std::is_signed<T>::value) {
      if (tl.type != SML_INT) {
        ESP_LOGW(TAG, "unexpected node type %u when expecting a signed integer node %s", tl.type, name);
        this->error_ = true;
        return false;
      }
    } else {
      if (tl.type != SML_UINT && tl.type != SML_BOOL) {
        ESP_LOGW(TAG, "unexpected node type %u when expecting a unsigned integer node %s", tl.type, name);
        this->error_ = true;
        return false;
      }
    }
    result = to_int<T>(this->ptr_, this->ptr_ + tl.length);
    this->ptr_ += tl.length;
    return true;
  }

  bool read_octet_string(byte_span &result, uint16_t &node_type) {
    auto tl = this->read_type_length();
    if (tl.type == SML_LIST) {
      ESP_LOGW(TAG, "unexpected list node when expecting a primitive node");
      this->error_ = true;
      return false;
    }
    result = byte_span(this->ptr_, tl.length);
    node_type = tl.type;
    this->ptr_ += tl.length;
    return true;
  }

  bool read_octet_string(byte_span &result, const char *name) {
    uint16_t node_type;
    if (!this->read_octet_string(result, node_type)) {
      return false;
    }
    if (node_type != SML_OCTET) {
      ESP_LOGW(TAG, "unexpected node type %u when expecting an octet string node %s", node_type, name);
      this->error_ = true;
      return false;
    }
    return true;
  }

 private:
  uint8_t const *ptr_;
  uint8_t const *const end_;
  bool error_;
};

static void process_get_list_response(Parser &parser, const std::function<void(const ObisInfo &)> &callback) {
  byte_span server_id;
  (void) parser.read_list_length("get_list_response");  // Ignore get_list_response length
  parser.skip_nodes(1);                                 // Skip clientId field
  parser.read_octet_string(server_id, "server id");
  parser.skip_nodes(2);  // Skip listName & actSensorTime fields
  auto val_list_length = parser.read_list_length("value list");
  for (int i = 0; i < val_list_length; i++) {
    auto val_entry_length = parser.read_list_length("value entry");
    if (val_entry_length < 6) {
      ESP_LOGW(TAG, "unexpected node length when expecting a value entry");
      break;
    }
    ObisInfo info;
    info.server_id = server_id;
    parser.read_octet_string(info.code, "value code");
    parser.skip_nodes(2);  // Skip status & valTime fields
    parser.read_int(info.unit, "value unit");
    parser.read_int(info.scaler, "value scaler");
    parser.read_octet_string(info.value, info.value_type);
    parser.skip_nodes(val_entry_length - 6);  // Skip remaining fields
    if (parser.error()) {
      break;
    }
    callback(info);
  }
}

void for_each_obis_info(uint8_t const *begin, uint8_t const *end,
                        const std::function<void(const ObisInfo &)> &callback) {
  Parser parser(begin, end);
  while (parser) {
    auto message_length = parser.read_list_length("message");
    if (message_length < 4) {
      ESP_LOGW(TAG, "unexpected node length %d when expecting a message", message_length);
      break;
    }
    parser.skip_nodes(3);  // Skip transactionId, groupNo & abortOnError fields
    auto message_body_length = parser.read_list_length("message body");
    if (message_body_length < 2) {
      ESP_LOGW(TAG, "unexpected node length %d when expecting a message body", message_body_length);
      break;
    }
    uint16_t message_type;
    if (parser.read_int(message_type, "message type")) {
      ESP_LOGVV(TAG, "Processed SML message %d", message_type);
      if (message_type == SML_GET_LIST_RES) {
        process_get_list_response(parser, callback);
      }
    }
    parser.skip_until_next_message();
  }
}

std::string bytes_repr(const byte_span &buffer) {
  std::string repr;
  for (auto const value : buffer) {
    repr += str_sprintf("%02x", value & 0xff);
  }
  return repr;
}

uint64_t bytes_to_uint(const byte_span &buffer) { return to_int<uint64_t>(buffer.begin(), buffer.end()); }

int64_t bytes_to_int(const byte_span &buffer) { return to_int<int64_t>(buffer.begin(), buffer.end()); }

std::string bytes_to_string(const byte_span &buffer) { return std::string(buffer.begin(), buffer.end()); }

std::string ObisInfo::code_repr() const {
  return str_sprintf("%d-%d:%d.%d.%d", this->code[0], this->code[1], this->code[2], this->code[3], this->code[4]);
}

}  // namespace sml
}  // namespace esphome
