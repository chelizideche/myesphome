#include "decompressor.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cinttypes>

namespace esphome {
namespace store_yaml {

static const char *const TAG = "store_yaml";

Decompressor::Decompressor(const uint8_t *ptr, size_t len) {
  this->data_ptr_ = ptr;
  this->data_len_ = len;
  this->reset();
}

void Decompressor::reset() {
  this->pos_ = 0;
  this->size_ = 0;
  this->buff_ = 0;
  this->codes_.clear();
  this->codes_.reserve(256);
  for (uint16_t i = 0; i < 256; i++) {
    this->codes_.push_back(Entry{.p = 0, .c = (uint8_t) i});
  }
  this->code_width_ = 9;  // log2next + 1
}

bool Decompressor::is_eof() const { return this->pos_ >= this->data_len_; }

uint32_t Decompressor::get_bits_(size_t bits) {
  if (this->is_eof()) {
    return UINT32_MAX;
  }

  while (this->size_ < bits) {
    this->buff_ = (this->buff_ << 8) | progmem_read_byte(&this->data_ptr_[this->pos_++]);
    this->size_ += 8;
  }

  uint32_t value = (this->buff_ >> (this->size_ - bits)) & ((1 << bits) - 1);
  this->size_ -= bits;
  this->buff_ &= (1 << this->size_) - 1;
  return value;
}

const Entry *Decompressor::get_entry_(uint16_t &code) {
  if (this->codes_.size() == ((1 << this->code_width_) - 1)) {
    this->code_width_++;
  }
  code = (uint16_t) this->get_bits_(this->code_width_);
  if (code < this->codes_.size()) {
    return &this->codes_[code];
  }
  if (code != this->codes_.size()) {
    this->pos_ = this->data_len_;  // error in input, set eof
  }
  return nullptr;
}

std::string Decompressor::get_string_(const Entry *entry) const {
  std::string s(1, (char) entry->c);
  while (entry->p != 0) {
    if (entry->p >= this->codes_.size()) {
      ESP_LOGE(TAG, "entry->p %" PRIu16 " not found", entry->p);
      break;
    }
    entry = &this->codes_[entry->p];
    s += (char) entry->c;
  }
  return std::string(s.rbegin(), s.rend());
}

std::string Decompressor::get_first() {
  this->reset();
  uint16_t code = 0;
  const Entry *entry = this->get_entry_(code);
  std::string s = this->get_string_(entry);
  this->prev_.c = (uint32_t) s[0];
  this->prev_.p = code;
  return s;
}

std::string Decompressor::get_next() {
  uint16_t code = 0;
  const Entry *entry = this->get_entry_(code);
  if (entry == nullptr)
    entry = &this->prev_;
  std::string s = this->get_string_(entry);
  this->prev_.c = s[0];
  this->codes_.push_back(this->prev_);
  this->prev_.p = code;
  if (this->is_eof()) {
    // free code table, no longer needed
    this->codes_.clear();
  }
  return s;
}

RowDecompressor::RowDecompressor(const uint8_t *ptr, size_t len) : Decompressor(ptr, len) {
  this->yaml_ = this->get_first();
}

bool RowDecompressor::get_row(std::string &row) {
  while (!this->is_eof() && this->yaml_.find('\n') == std::string::npos) {
    this->yaml_ += this->get_next();
  }

  size_t pos = this->yaml_.find('\n');
  if (pos != std::string::npos) {
    row = this->yaml_.substr(0, pos);
    this->yaml_ = std::string(this->yaml_.begin() + pos + 1, this->yaml_.end());
    return true;
  }

  if (this->is_eof() && !this->yaml_.empty()) {
    // no new line at the end of the file
    row = this->yaml_;
    this->yaml_.clear();
    return true;
  }

  return false;
}

}  // namespace store_yaml
}  // namespace esphome
