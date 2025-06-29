#pragma once
#ifdef USE_ESP_IDF
#ifdef USE_WEBSERVER_OTA

#include <string>
#include <vector>
#include <cstring>

namespace esphome {
namespace web_server_idf {

// Multipart form data parser for ESP-IDF
// Implements RFC 7578 compliant multipart/form-data parsing
class MultipartParser {
 public:
  static constexpr const char *MULTIPART_BOUNDARY_PREFIX = "--";

  enum State : uint8_t { BOUNDARY_SEARCH, HEADERS, CONTENT, DONE, ERROR };

  struct Part {
    std::string name;
    std::string filename;
    std::string content_type;
    const uint8_t *data;
    size_t length;
  };

  explicit MultipartParser(const std::string &boundary)
      : boundary_(MULTIPART_BOUNDARY_PREFIX + boundary),
        state_(BOUNDARY_SEARCH),
        content_length_(0),
        part_ready_(false) {}

  // Process incoming data chunk
  // Returns true if a complete part is available
  bool parse(const uint8_t *data, size_t len);

  // Get the current part if available
  bool get_current_part(Part &part) const;

  // Consume the current part and move to next
  void consume_part();

  State get_state() const { return state_; }
  bool is_done() const { return state_ == DONE; }
  bool has_error() const { return state_ == ERROR; }

  // Reset parser for reuse
  void reset();

 private:
  bool find_boundary();
  bool parse_headers();
  void process_header_line(const std::string &line);
  bool extract_content();

  std::string read_line();
  size_t find_pattern(const uint8_t *pattern, size_t pattern_len, size_t start = 0) const;

  std::string boundary_;
  State state_;
  std::vector<uint8_t> buffer_;

  // Current part info
  std::string current_name_;
  std::string current_filename_;
  std::string current_content_type_;
  size_t content_length_{0};
  bool part_ready_{false};
};

}  // namespace web_server_idf
}  // namespace esphome
#endif  // USE_WEBSERVER_OTA
#endif  // USE_ESP_IDF
