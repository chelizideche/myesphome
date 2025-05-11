#include "logger.h"
#include <cinttypes>
#ifdef USE_ESPHOME_LOG_BUFFER
#include <memory>
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
  // Only get thread name if needed (non-main task) and log buffer is enabled
  const char *thread_name = pcTaskGetName(current_task);

  // Use the log buffer for messages from non-main tasks
  size_t capacity;
  void *message_token = nullptr;

  // Debug counters for buffer acquisition
  static uint32_t prepare_attempt_count = 0;
  static uint32_t prepare_success_count = 0;
  static uint32_t prepare_fail_count = 0;
  prepare_attempt_count++;

  char *buffer = this->log_buffer_->prepare_message(static_cast<uint8_t>(level), tag, static_cast<uint16_t>(line),
                                                    thread_name, capacity, &message_token);

  // Print prepare stats less frequently
  if (prepare_attempt_count % 200 == 0 && this->baud_rate_ > 0) {
    char dbg[80];
    snprintf(dbg, sizeof(dbg), "MSGDBG: prepare %u success %u fail %u", prepare_attempt_count, prepare_success_count,
             prepare_fail_count);
    this->write_msg_(dbg);
  }

  if (buffer == nullptr) {
    prepare_fail_count++;
  }

  if (buffer != nullptr) {
    prepare_success_count++;
    // Format the message into the ring buffer
    int ret = vsnprintf(buffer, capacity, format, args);

    // Calculate actual text length based on vsnprintf result:
    // - zero or negative: empty or encoding error, skip message
    // - exceeds capacity: truncated, use capacity-1
    // - otherwise: use actual length
    if (ret <= 0) {
      // Empty message or error in vsnprintf, cancel the prepared message
      this->log_buffer_->release_message(message_token);
      recursion_guard_.store(false, std::memory_order_release);
      return;
    }

    size_t text_length = (static_cast<size_t>(ret) >= capacity) ? capacity - 1 : ret;

    // Remove trailing newlines
    while (text_length > 0 && buffer[text_length - 1] == '\n') {
      text_length--;
    }

    // Check if we have any text to log after processing
    if (text_length > 0) {
      this->log_buffer_->commit_message(text_length, message_token);

      // Debug message for successful commits - very infrequent output
      static uint32_t commit_count = 0;
      if ((++commit_count % 100) == 0 && this->baud_rate_ > 0) {
        char cbuf[60];
        snprintf(cbuf, sizeof(cbuf), "MSGDBG: Committed message #%u len=%u", commit_count, (unsigned int) text_length);
        this->write_msg_(cbuf);
      }

    } else {
      // No text to log, cancel the prepared message
      this->log_buffer_->release_message(message_token);
    }

    recursion_guard_.store(false, std::memory_order_release);
    return;
  }
#endif  // USE_ESPHOME_LOG_BUFFER

  // Log buffer is disabled or full, use emergency console logging
  // Check if logging is disabled
  if (this->baud_rate_ > 0) {
    // Emergency console-only fallback - MUST be stack allocated for thread safety
    char emergency_buffer[160];
    int buffer_at = 0;

    this->format_log_to_buffer_(level, tag, line, format, args, emergency_buffer, &buffer_at, 160);

    // Remove trailing newlines to prevent double empty lines
    while (buffer_at > 0 && emergency_buffer[buffer_at - 1] == '\n') {
      buffer_at--;
    }

    // Add null terminator before sending to output
    if (buffer_at < 160)
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
  // Add debug counter for loop calls
  static uint32_t loop_count = 0;
  loop_count++;

  // Occasional diagnostic about loop calls
  if (loop_count % 1000 == 0) {
    char dbg_buf[80];
    snprintf(dbg_buf, sizeof(dbg_buf), "DBGINFO: Logger loop called %u times", loop_count);
    if (this->baud_rate_ > 0) {
      this->write_msg_(dbg_buf);
    }
  }

  logger::LogBuffer::LogMessage *message;
  const char *text;
  void *received_token;
  bool any_messages = false;
  int msg_count = 0;

  // Process up to 10 messages per loop call to avoid blocking too long
  while (msg_count < 10 && this->log_buffer_->borrow_message(&message, &text, &received_token)) {
    any_messages = true;
    msg_count++;

    this->tx_buffer_at_ = 0;
    this->write_header_to_buffer_(message->level, message->tag, message->line, this->tx_buffer_, &this->tx_buffer_at_,
                                  this->tx_buffer_size_, message->thread_name);
    this->write_body_to_buffer_(text, message->text_length, this->tx_buffer_, &this->tx_buffer_at_,
                                this->tx_buffer_size_);
    this->write_footer_to_buffer_(this->tx_buffer_, &this->tx_buffer_at_, this->tx_buffer_size_);
    this->log_message_(message->level, message->tag);

    this->log_buffer_->release_message(received_token);
  }

  // If messages were processed, occasionally print the count
  if (msg_count > 0 && loop_count % 50 == 0 && this->baud_rate_ > 0) {
    char count_buf[60];
    snprintf(count_buf, sizeof(count_buf), "DBGINFO: Processed %d buffered messages", msg_count);
    this->write_msg_(count_buf);
  }

  // If no messages were processed for 10 consecutive calls, something might be wrong
  static uint8_t empty_count = 0;
  if (!any_messages) {
    if (++empty_count >= 10) {
      // Log directly to serial using emergency path - won't go through the buffer
      char emergency_buffer[80];
      snprintf(emergency_buffer, sizeof(emergency_buffer),
               "DBGINFO: No log messages in ring buffer for %u consecutive calls", empty_count);
      if (this->baud_rate_ > 0) {
        this->write_msg_(emergency_buffer);
      }
    }
  } else {
    empty_count = 0;
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
