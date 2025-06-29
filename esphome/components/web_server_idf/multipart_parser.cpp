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

  bool made_progress = true;
  while (made_progress && state_ != DONE && state_ != ERROR && !buffer_.empty()) {
    made_progress = false;

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
          content_start_ = 0;  // Content starts at current buffer position
          made_progress = true;
        }
        break;

      case CONTENT:
        if (extract_content()) {
          // Content is ready, return to caller
          return true;
        }
        break;

      default:
        break;
    }
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
  content_start_ = 0;
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
  content_start_ = 0;
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
  while (true) {
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

    // Parse Content-Disposition header (case-insensitive)
    if (str_startswith_case_insensitive(line, "content-disposition:")) {
      // Extract name parameter
      std::string name = extract_header_param(line, "name");
      if (!name.empty()) {
        current_name_ = name;
      }

      // Extract filename parameter if present
      std::string filename = extract_header_param(line, "filename");
      if (!filename.empty()) {
        current_filename_ = filename;
      }
    }
    // Parse Content-Type header (case-insensitive)
    else if (str_startswith_case_insensitive(line, "content-type:")) {
      // Find the colon and skip it
      size_t colon_pos = line.find(':');
      if (colon_pos != std::string::npos) {
        current_content_type_ = line.substr(colon_pos + 1);
        // Trim leading whitespace
        size_t start = current_content_type_.find_first_not_of(" \t");
        if (start != std::string::npos) {
          current_content_type_ = current_content_type_.substr(start);
        } else {
          current_content_type_.clear();
        }
        // Trim trailing whitespace
        size_t end = current_content_type_.find_last_not_of(" \t\r\n");
        if (end != std::string::npos) {
          current_content_type_ = current_content_type_.substr(0, end + 1);
        }
      }
    }
  }
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
  auto crlf_pos = find_pattern(reinterpret_cast<const uint8_t *>(CRLF_STR), CRLF_LENGTH);
  if (crlf_pos == std::string::npos) {
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