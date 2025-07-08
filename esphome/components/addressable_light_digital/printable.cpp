#include "printable.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace addressable_light_digital {
static const char *const TAG = "text_display";

uint8_t Printable::print(uint8_t pos, const char *str) { return this->print_core(pos, str); }

uint8_t Printable::print(const char *str) { return this->print(0, str); }

uint8_t Printable::printf(uint8_t pos, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char *buffer = nullptr;
  int ret = vsnprintf(nullptr, 0, format, arg);
  if (ret > 0) {
    buffer = new char[ret + 1];
    va_end(arg);
    va_start(arg, format);
    vsnprintf(buffer, ret + 1, format, arg);
    va_end(arg);
    uint8_t result = this->print(pos, buffer);
    delete[] buffer;
    return result;
  }
  va_end(arg);
  return 0;
}

uint8_t Printable::printf(const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char *buffer = nullptr;
  int ret = vsnprintf(nullptr, 0, format, arg);
  if (ret > 0) {
    buffer = new char[ret + 1];
    va_end(arg);
    va_start(arg, format);
    vsnprintf(buffer, ret + 1, format, arg);
    va_end(arg);
    uint8_t result = this->print(buffer);
    delete[] buffer;
    return result;
  }
  va_end(arg);
  return 0;
}

uint8_t Printable::strftime(uint8_t pos, const char *format, ESPTime time) {
  // Start with a reasonable size and grow if needed
  size_t buffer_size = 64;
  char *buffer = new char[buffer_size];
  size_t ret = time.strftime(buffer, buffer_size, format);

  // If truncation occurred, try with a larger buffer
  while (ret == 0 && buffer_size < 512) {
    delete[] buffer;
    buffer_size *= 2;
    buffer = new char[buffer_size];
    ret = time.strftime(buffer, buffer_size, format);
  }

  if (ret > 0) {
    uint8_t result = this->print(pos, buffer);
    delete[] buffer;
    return result;
  }
  delete[] buffer;
  return 0;
}

uint8_t Printable::strftime(const char *format, ESPTime time) { return this->strftime(0, format, time); }

}  // namespace addressable_light_digital
}  // namespace esphome
