#include "logger_buffer.h"

#ifdef USE_ESPHOME_LOG_BUFFER

namespace esphome {
namespace logger {

LogBuffer::LogBuffer(size_t total_buffer_size) : message_prepared_(false), read_index_(0), write_index_(0) {
  // Calculate max message size based on typical log message
  const size_t header_size = sizeof(LogMessage);

  // Set max message size to accommodate larger messages
  max_message_size_ = header_size + MAX_MESSAGE_TEXT_SIZE + 1;  // +1 for null terminator

  // Ensure minimum buffer size for at least 2 full-size messages
  if (total_buffer_size < max_message_size_ * 2) {
    total_buffer_size = max_message_size_ * 2;
  }

  // Allocate the single unified buffer
  buffer_size_ = total_buffer_size;
  esphome::RAMAllocator<char> allocator;
  buffer_ = allocator.allocate(buffer_size_);

  // Initialize buffer memory
  memset(buffer_, 0, buffer_size_);

  // Initialize pointers to the beginning of the buffer
  read_pos_ = reinterpret_cast<LogMessage *>(buffer_);
  write_pos_ = reinterpret_cast<LogMessage *>(buffer_);
  prepared_pos_ = nullptr;

  // Initialize first message header to have zero length
  read_pos_->text_length = 0;
}

LogBuffer::~LogBuffer() {
  if (buffer_ != nullptr) {
    // Free the buffer
    esphome::RAMAllocator<char> allocator;
    allocator.deallocate(buffer_, buffer_size_);
    buffer_ = nullptr;
    read_pos_ = nullptr;
    write_pos_ = nullptr;
    prepared_pos_ = nullptr;
  }
}

char *LogBuffer::prepare_message(uint8_t level, const char *tag, uint16_t line, const char *thread_name,
                                 size_t &capacity) {
  // Try to set message_prepared_ from false to true atomically
  bool expected = false;
  if (!message_prepared_.compare_exchange_strong(expected, true, std::memory_order_acquire)) {
    // Someone else is already preparing a message
    return nullptr;
  }

  // Calculate space needed for header + minimum text size
  const size_t min_space_needed = sizeof(LogMessage) + MIN_USEFUL_MESSAGE_SIZE + 1;  // +1 for null terminator

  // Calculate available space in the buffer
  const char *buffer_end = buffer_ + buffer_size_;
  const char *read_pos_ptr = reinterpret_cast<const char *>(read_pos_);
  const char *write_pos_ptr = reinterpret_cast<const char *>(write_pos_);

  size_t available_space;
  if (write_pos_ptr >= read_pos_ptr) {
    // Write is ahead of read or at same position
    const size_t space_to_end = buffer_end - write_pos_ptr;

    if (space_to_end >= min_space_needed) {
      // Enough space at the end
      available_space = space_to_end;
      prepared_pos_ = write_pos_;
    } else {
      // Not enough space at the end, check if we can wrap
      if (read_pos_ptr - buffer_ >= min_space_needed) {
        // Wrap to the beginning of the buffer
        prepared_pos_ = reinterpret_cast<LogMessage *>(buffer_);
        available_space = read_pos_ptr - buffer_;
      } else {
        // Not enough space for a message
        return release_lock_and_return_null_();
      }
    }
  } else {
    // Read is ahead of write
    available_space = read_pos_ptr - write_pos_ptr;
    if (available_space < min_space_needed) {
      // Not enough space
      return release_lock_and_return_null_();
    }
    prepared_pos_ = write_pos_;
  }

  // Limit capacity to max_message_size_
  capacity = std::min(available_space - sizeof(LogMessage) - 1, max_message_size_ - sizeof(LogMessage) - 1);

  // Fill in the message header
  prepared_pos_->level = level;
  prepared_pos_->tag = tag;
  prepared_pos_->line = line;
  prepared_pos_->thread_name = thread_name;
  prepared_pos_->text_length = 0;  // Will be filled in commit

  // Return a pointer to where text should be written
  return prepared_pos_->text_data();
}

void LogBuffer::commit_message(size_t text_length) {
  if (!message_prepared_.load(std::memory_order_relaxed) || prepared_pos_ == nullptr || text_length == 0) {
    // Reset prepared flag and abort if zero text length or no prepared message
    message_prepared_.store(false, std::memory_order_release);
    return;
  }

  // Limit text length to maximum allowed
  const size_t max_allowed_text = max_message_size_ - sizeof(LogMessage) - 1;
  if (text_length > max_allowed_text) {
    text_length = max_allowed_text;
  }

  // Store the text length
  prepared_pos_->text_length = text_length;

  // Add null terminator after the text
  char *text_data = prepared_pos_->text_data();
  text_data[text_length] = '\0';

  // Calculate next message position
  const char *buffer_end = buffer_ + buffer_size_;
  const size_t total_msg_size = prepared_pos_->total_size();
  char *next_msg_pos = reinterpret_cast<char *>(prepared_pos_) + total_msg_size;

  // Check if we need to wrap to the beginning of the buffer
  if (next_msg_pos + sizeof(LogMessage) >= buffer_end) {
    next_msg_pos = buffer_;
  }

  // Update write position to the next message slot
  write_pos_ = reinterpret_cast<LogMessage *>(next_msg_pos);

  // Increment the write index atomically with release semantics
  write_index_.fetch_add(1, std::memory_order_release);

  // Reset prepared flag
  message_prepared_.store(false, std::memory_order_release);
}

bool LogBuffer::borrow_message(LogMessage **message, const char **text) {
  // Check if buffer is empty using atomic indices
  // Use acquire semantics to synchronize with the release in commit_message
  size_t read_idx = read_index_.load(std::memory_order_acquire);
  size_t write_idx = write_index_.load(std::memory_order_acquire);

  if (read_idx == write_idx) {
    return false;  // Buffer empty
  }

  // Get message and its text
  *message = read_pos_;
  *text = read_pos_->text_data();

  return true;
}

void LogBuffer::release_message() {
  // Check if buffer is empty using atomic indices
  size_t read_idx = read_index_.load(std::memory_order_acquire);
  size_t write_idx = write_index_.load(std::memory_order_acquire);

  if (read_idx == write_idx) {
    return;  // Buffer empty
  }

  // Calculate next read position based on the current message's size
  const char *buffer_end = buffer_ + buffer_size_;
  size_t msg_size = read_pos_->total_size();
  char *next_read_pos = reinterpret_cast<char *>(read_pos_) + msg_size;

  // Check if we need to wrap around to beginning of buffer
  if (next_read_pos >= buffer_end || next_read_pos + sizeof(LogMessage) > buffer_end) {
    next_read_pos = buffer_;
  }

  // Update read position to the next message
  read_pos_ = reinterpret_cast<LogMessage *>(next_read_pos);

  // Increment the read index atomically
  read_index_.fetch_add(1, std::memory_order_release);
}

void LogBuffer::cancel_prepare() { message_prepared_.store(false, std::memory_order_release); }

}  // namespace logger
}  // namespace esphome

#endif  // USE_ESPHOME_LOG_BUFFER
