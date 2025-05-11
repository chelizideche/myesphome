#include "logger.h"
#include <cinttypes>
#ifdef USE_ESPHOME_LOG_BUFFER
#include <memory>
#include "logger_buffer.h"
#else
// When log buffer is not enabled, define this constant locally
constexpr size_t LOG_MSG_SIZE_WITH_NULL = 160;
#endif

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace logger {

static const char *const TAG = "logger";

#ifdef USE_ESP32
// Implementation for multi-core platforms with full atomic support (ESP32)
// Uses direct logging for the main task for maximum performance
// Uses lock-free ring buffer with atomic operations for logging from other tasks
void HOT Logger::log_vprintf_(int level, const char *tag, int line, const char *format, va_list args) {  // NOLINT
  if (level > this->level_for(tag) || recursion_guard_.load(std::memory_order_relaxed))
    return;
  recursion_guard_.store(true, std::memory_order_relaxed);

  TaskHandle_t current_task = xTaskGetCurrentTaskHandle();

  if (current_task == main_task_) {
    this->log_message_sync_(level, tag, line, format, args);
    recursion_guard_.store(false, std::memory_order_release);
    return;
  }

#ifdef USE_ESPHOME_LOG_BUFFER

  // Use the log buffer for messages from non-main tasks
  // If we successfully used the log buffer, return
  if (this->log_buffer_->send_message_thread_safe(static_cast<uint8_t>(level), tag, static_cast<uint16_t>(line), format,
                                                  args)) {
    recursion_guard_.store(false, std::memory_order_release);
    return;
  }
  // Otherwise fall through to emergency console
#endif  // USE_ESPHOME_LOG_BUFFER

  // Log buffer is disabled or full, use emergency console logging
  // Check if logging is disabled
  if (this->baud_rate_ > 0) {
    // Emergency console-only fallback - MUST be stack allocated for thread safety
    char emergency_buffer[LOG_MSG_SIZE_WITH_NULL];
    int buffer_at = 0;

    this->format_log_to_buffer_(level, tag, line, format, args, emergency_buffer, &buffer_at, LOG_MSG_SIZE_WITH_NULL);

    // Remove trailing newlines to prevent double empty lines
    while (buffer_at > 0 && emergency_buffer[buffer_at - 1] == '\n') {
      buffer_at--;
    }

    // Add null terminator before sending to output
    if (buffer_at < LOG_MSG_SIZE_WITH_NULL)
      emergency_buffer[buffer_at] = '\0';

    // Send directly to output, skip callbacks
    this->write_msg_(emergency_buffer);
  }
  recursion_guard_.store(false, std::memory_order_release);
}
#endif  // USE_ESP32

#ifndef USE_ESP32
// Implementation for platforms that do not support atomic operations
// or have to consider logging in other tasks
void HOT Logger::log_vprintf_(int level, const char *tag, int line, const char *format, va_list args) {  // NOLINT
  if (level > this->level_for(tag) || recursion_guard_)
    return;

  recursion_guard_ = true;
  this->log_message_sync_(level, tag, line, format, args);
  recursion_guard_ = false;
}
#endif  // !USE_ESP32

#ifdef USE_STORE_LOG_STR_IN_FLASH
// Implementation for ESP8266 with flash string support
void Logger::log_vprintf_(int level, const char *tag, int line, const __FlashStringHelper *format,
                          va_list args) {  // NOLINT
  if (level > this->level_for(tag) || recursion_guard_)
    return;

  recursion_guard_ = true;
  this->tx_buffer_at_ = 0;
  // copy format string
  auto *format_pgm_p = reinterpret_cast<const uint8_t *>(format);
  char ch = '.';
  while (this->tx_buffer_at_ < this->tx_buffer_size_ && ch != '\0') {
    this->tx_buffer_[this->tx_buffer_at_++] = ch = (char) progmem_read_byte(format_pgm_p++);
  }
  // Buffer full form copying format
  if (this->tx_buffer_at_ >= this->tx_buffer_size_)
    return;

  // length of format string, includes null terminator
  uint32_t offset = this->tx_buffer_at_;
  this->format_log_to_buffer_(level, tag, line, this->tx_buffer_, args, this->tx_buffer_, &this->tx_buffer_at_,
                              this->tx_buffer_size_);
  this->log_message_(level, tag, offset);
  recursion_guard_ = false;
}
#endif  // USE_STORE_LOG_STR_IN_FLASH

inline int HOT Logger::level_for(const char *tag) {
  auto it = this->log_levels_.find(tag);
  if (it != this->log_levels_.end())
    return it->second;
  return this->current_level_;
}

void HOT Logger::log_message_(int level, const char *tag, int offset) {
  // Remove all trailing newlines to prevent double empty lines
  while (this->tx_buffer_at_ > 0 && this->tx_buffer_[this->tx_buffer_at_ - 1] == '\n') {
    this->tx_buffer_at_--;
  }

  // make sure null terminator is present (inlined set_null_terminator_)
  this->tx_buffer_[this->tx_buffer_at_] = '\0';

  const char *msg = this->tx_buffer_ + offset;

  if (this->baud_rate_ > 0) {
    this->write_msg_(msg);
  }

#ifdef USE_ESP32
  // Suppress network-logging if memory constrained, but still log to serial
  // ports. In some configurations (eg BLE enabled) there may be some transient
  // memory exhaustion, and trying to log when OOM can lead to a crash. Skipping
  // here usually allows the stack to recover instead.
  // See issue #1234 for analysis.
  if (xPortGetFreeHeapSize() < 2048)
    return;
#endif

  this->log_callback_.call(level, tag, msg);
}

Logger::Logger(uint32_t baud_rate, size_t tx_buffer_size) : baud_rate_(baud_rate), tx_buffer_size_(tx_buffer_size) {
  // add 1 to buffer size for null terminator
  this->tx_buffer_ = new char[this->tx_buffer_size_ + 1];  // NOLINT
#if defined(USE_ESP32) || defined(USE_LIBRETINY)
  this->main_task_ = xTaskGetCurrentTaskHandle();
#endif
}

#ifdef USE_ESPHOME_LOG_BUFFER
void Logger::init_log_buffer(size_t total_buffer_size) {
  this->log_buffer_ = esphome::make_unique<logger::LogBuffer>(total_buffer_size);
}

void Logger::report_buffer_stats() {
  // Debug function removed
}
#endif

#if defined(USE_LOGGER_USB_CDC) || defined(USE_ESP32)
void Logger::loop() {
#ifdef USE_LOGGER_USB_CDC
#ifdef USE_ARDUINO
  if (this->uart_ == UART_SELECTION_USB_CDC) {
    static bool opened = false;
    if (opened == Serial) {
      return;
    }
    if (false == opened) {
      App.schedule_dump_config();
    }
    opened = !opened;
  }
#endif
#endif

#ifdef USE_ESPHOME_LOG_BUFFER
  // Process any buffered messages when available
  if (this->log_buffer_->has_messages()) {
    logger::LogBuffer::LogMessage *message;
    const char *text;
    void *received_token;

    // Process messages from the buffer
    while (this->log_buffer_->borrow_message_main_loop(&message, &text, &received_token)) {
      this->tx_buffer_at_ = 0;
      const char *thread_name = nullptr;
      if (message->task_handle != nullptr) {
        thread_name = pcTaskGetName(message->task_handle);
      }
      this->write_header_to_buffer_(message->level, message->tag, message->line, this->tx_buffer_, &this->tx_buffer_at_,
                                    this->tx_buffer_size_, thread_name);
      this->write_body_to_buffer_(text, message->text_length, this->tx_buffer_, &this->tx_buffer_at_,
                                  this->tx_buffer_size_);
      this->write_footer_to_buffer_(this->tx_buffer_, &this->tx_buffer_at_, this->tx_buffer_size_);
      this->log_message_(message->level, message->tag);

      // Use main loop version of release_message that updates the counter tracking
      this->log_buffer_->release_message_main_loop(received_token);
    }
  }
#endif
}
#endif

void Logger::set_baud_rate(uint32_t baud_rate) { this->baud_rate_ = baud_rate; }
void Logger::set_log_level(const std::string &tag, int log_level) { this->log_levels_[tag] = log_level; }

#if defined(USE_ESP32) || defined(USE_ESP8266) || defined(USE_RP2040) || defined(USE_LIBRETINY)
UARTSelection Logger::get_uart() const { return this->uart_; }
#endif

void Logger::add_on_log_callback(std::function<void(int, const char *, const char *)> &&callback) {
  this->log_callback_.add(std::move(callback));
}
float Logger::get_setup_priority() const { return setup_priority::BUS + 500.0f; }
static const char *const LOG_LEVELS[] = {"NONE", "ERROR", "WARN", "INFO", "CONFIG", "DEBUG", "VERBOSE", "VERY_VERBOSE"};

void Logger::dump_config() {
  ESP_LOGCONFIG(TAG, "Logger:");
  ESP_LOGCONFIG(TAG, "  Max Level: %s", LOG_LEVELS[ESPHOME_LOG_LEVEL]);
  ESP_LOGCONFIG(TAG, "  Initial Level: %s", LOG_LEVELS[this->current_level_]);
#ifndef USE_HOST
  ESP_LOGCONFIG(TAG, "  Log Baud Rate: %" PRIu32, this->baud_rate_);
  ESP_LOGCONFIG(TAG, "  Hardware UART: %s", get_uart_selection_());
#endif

  for (auto &it : this->log_levels_) {
    ESP_LOGCONFIG(TAG, "  Level for '%s': %s", it.first.c_str(), LOG_LEVELS[it.second]);
  }
}

void Logger::set_log_level(int level) {
  if (level > ESPHOME_LOG_LEVEL) {
    level = ESPHOME_LOG_LEVEL;
    ESP_LOGW(TAG, "Cannot set log level higher than pre-compiled %s", LOG_LEVELS[ESPHOME_LOG_LEVEL]);
  }
  this->current_level_ = level;
  this->level_callback_.call(level);
}

Logger *global_logger = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace logger
}  // namespace esphome
