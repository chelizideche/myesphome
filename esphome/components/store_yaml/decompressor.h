#pragma once

#include <string>
#include <vector>

namespace esphome {
namespace store_yaml {

struct Entry {
  // uint32_t p : 24;
  // uint32_t c : 8;
  uint16_t p;  // save 1 byte, this reduces the number of code words to 65536, more than enough for configs
  uint8_t c;
} __attribute__((packed));

class Decompressor {
 protected:
  const uint8_t *data_ptr_;
  size_t data_len_;
  std::vector<Entry> codes_;
  size_t pos_;
  uint8_t size_;
  uint32_t buff_;
  uint8_t code_width_;
  Entry prev_;

  uint32_t get_bits_(size_t bits);
  const Entry *get_entry_(uint16_t &code);
  std::string get_string_(const Entry *entry) const;

 public:
  Decompressor(const uint8_t *ptr, size_t len);

  void reset();
  bool is_eof() const;
  std::string get_first();
  std::string get_next();
};

class RowDecompressor : public Decompressor {
 protected:
  std::string yaml_;

 public:
  RowDecompressor(const uint8_t *ptr, size_t len);

  bool get_row(std::string &row);
};

}  // namespace store_yaml
}  // namespace esphome
