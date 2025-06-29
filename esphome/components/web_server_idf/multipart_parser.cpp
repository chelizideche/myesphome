#ifdef USE_ESP_IDF
#ifdef USE_WEBSERVER_OTA
#include "multipart_parser.h"
#include "multipart_parser_utils.h"
#include "esphome/core/log.h"

namespace esphome {
namespace web_server_idf {

static const char *const TAG = "multipart_parser";

// Constants for multipart parsing
static constexpr size_t CRLF_LENGTH = 2;
static constexpr size_t MIN_BOUNDARY_BUFFER = 4;  // Extra bytes to keep for split boundary detection
static constexpr const char *CRLF_STR = "\r\n";

bool MultipartParser::parse(const uint8_t *data, size_t len) {
  // Append new data to buffer
  if (data && len > 0) {
    buffer_.insert(buffer_.end(), data, data + len);
  }

  // Limit iterations to prevent infinite loops
  static constexpr size_t MAX_ITERATIONS = 10;
  size_t iterations = 0;

  bool made_progress = true;
  while (made_progress && state_ != DONE && state_ != ERROR && !buffer_.empty() && iterations < MAX_ITERATIONS) {
    made_progress = false;
    iterations++;

    switch (state_) {
      case BOUNDARY_SEARCH:
        if (find_boundary()) {
          state_ = HEADERS;
          made_progress = true;
        }
        break;

      case HEADERS:
        if (parse_headers()) {
          state_ = CONTENT;
          made_progress = true;
        }
        break;

      case CONTENT:
        if (extract_content()) {
          // Content is ready, return to caller
          return true;
        }
        // If we're waiting for more data in CONTENT state, exit the loop
        return false;

      default:
        ESP_LOGE(TAG, "Invalid parser state: %d", state_);
        state_ = ERROR;
        break;
    }
  }

  if (iterations >= MAX_ITERATIONS) {
    ESP_LOGW(TAG, "Parser reached maximum iterations, possible malformed data");
  }

  return part_ready_;
}

bool MultipartParser::get_current_part(Part &part) const {
  if (!part_ready_ || content_length_ == 0) {
    return false;
  }

  part.name = current_name_;
  part.filename = current_filename_;
  part.content_type = current_content_type_;
  part.data = buffer_.data();
  part.length = content_length_;

  return true;
}

void MultipartParser::consume_part() {
  if (!part_ready_) {
    return;
  }

  // Remove consumed data from buffer
  if (content_length_ < buffer_.size()) {
    buffer_.erase(buffer_.begin(), buffer_.begin() + content_length_);
  } else {
    buffer_.clear();
  }

  // Reset for next part
  part_ready_ = false;
  content_length_ = 0;
  current_name_.clear();
  current_filename_.clear();
  current_content_type_.clear();

  // Look for next boundary
  state_ = BOUNDARY_SEARCH;
}

void MultipartParser::reset() {
  buffer_.clear();
  state_ = BOUNDARY_SEARCH;
  part_ready_ = false;
  content_length_ = 0;
  current_name_.clear();
  current_filename_.clear();
  current_content_type_.clear();
}

bool MultipartParser::find_boundary() {
  // Look for boundary in buffer
  size_t boundary_pos = find_pattern(reinterpret_cast<const uint8_t *>(boundary_.c_str()), boundary_.length());

  if (boundary_pos == std::string::npos) {
    // Keep some data for next iteration to handle split boundaries
    if (buffer_.size() > boundary_.length() + MIN_BOUNDARY_BUFFER) {
      buffer_.erase(buffer_.begin(), buffer_.end() - boundary_.length() - MIN_BOUNDARY_BUFFER);
    }
    return false;
  }

  // Remove everything up to and including the boundary
  buffer_.erase(buffer_.begin(), buffer_.begin() + boundary_pos + boundary_.length());

  // Skip CRLF after boundary
  if (buffer_.size() >= CRLF_LENGTH && buffer_[0] == '\r' && buffer_[1] == '\n') {
    buffer_.erase(buffer_.begin(), buffer_.begin() + CRLF_LENGTH);
  }

  // Check if this is the end boundary
  if (buffer_.size() >= CRLF_LENGTH && buffer_[0] == '-' && buffer_[1] == '-') {
    state_ = DONE;
    return false;
  }

  return true;
}

bool MultipartParser::parse_headers() {
  // Limit header lines to prevent DOS attacks
  static constexpr size_t MAX_HEADER_LINES = 50;
  size_t header_count = 0;

  while (header_count < MAX_HEADER_LINES) {
    std::string line = read_line();
    if (line.empty()) {
      // Check if we have enough data for a line
      auto crlf_pos = find_pattern(reinterpret_cast<const uint8_t *>(CRLF_STR), CRLF_LENGTH);
      if (crlf_pos == std::string::npos) {
        return false;  // Need more data
      }
      // Empty line means headers are done
      buffer_.erase(buffer_.begin(), buffer_.begin() + CRLF_LENGTH);
      return true;
    }

    process_header_line(line);
    header_count++;
  }

  ESP_LOGW(TAG, "Too many headers in multipart data");
  state_ = ERROR;
  return false;
}

void MultipartParser::process_header_line(const std::string &line) {
  if (str_startswith_case_insensitive(line, "content-disposition:")) {
    // Extract name and filename parameters
    current_name_ = extract_header_param(line, "name");
    current_filename_ = extract_header_param(line, "filename");
  } else if (str_startswith_case_insensitive(line, "content-type:")) {
    current_content_type_ = extract_header_value(line);
  }
  // RFC 7578: Ignore any other Content-* headers
}

bool MultipartParser::extract_content() {
  // Look for next boundary
  std::string search_boundary = CRLF_STR + boundary_;
  size_t boundary_pos =
      find_pattern(reinterpret_cast<const uint8_t *>(search_boundary.c_str()), search_boundary.length());

  if (boundary_pos != std::string::npos) {
    // Found complete part
    content_length_ = boundary_pos;
    part_ready_ = true;
    return true;
  }

  // No boundary found yet, but we might have partial content
  // Keep enough bytes to ensure we don't split a boundary
  size_t safe_length = buffer_.size();
  if (safe_length > search_boundary.length() + MIN_BOUNDARY_BUFFER) {
    safe_length -= search_boundary.length() + MIN_BOUNDARY_BUFFER;
    if (safe_length > 0) {
      content_length_ = safe_length;
      // We have partial content but not complete yet
      return false;
    }
  }

  return false;
}

std::string MultipartParser::read_line() {
  // Limit line length to prevent excessive memory usage
  static constexpr size_t MAX_LINE_LENGTH = 4096;

  auto crlf_pos = find_pattern(reinterpret_cast<const uint8_t *>(CRLF_STR), CRLF_LENGTH);
  if (crlf_pos == std::string::npos) {
    // If we have too much data without CRLF, it's likely malformed
    if (buffer_.size() > MAX_LINE_LENGTH) {
      ESP_LOGW(TAG, "Header line too long, truncating");
      state_ = ERROR;
    }
    return "";
  }

  if (crlf_pos > MAX_LINE_LENGTH) {
    ESP_LOGW(TAG, "Header line exceeds maximum length");
    state_ = ERROR;
    return "";
  }

  std::string line(buffer_.begin(), buffer_.begin() + crlf_pos);
  buffer_.erase(buffer_.begin(), buffer_.begin() + crlf_pos + CRLF_LENGTH);
  return line;
}

size_t MultipartParser::find_pattern(const uint8_t *pattern, size_t pattern_len, size_t start) const {
  if (buffer_.size() < pattern_len + start) {
    return std::string::npos;
  }

  for (size_t i = start; i <= buffer_.size() - pattern_len; ++i) {
    if (memcmp(buffer_.data() + i, pattern, pattern_len) == 0) {
      return i;
    }
  }

  return std::string::npos;
}

}  // namespace web_server_idf
}  // namespace esphome
#endif  // USE_WEBSERVER_OTA
#endif  // USE_ESP_IDF
