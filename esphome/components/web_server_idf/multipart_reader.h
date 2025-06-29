#pragma once
#ifdef USE_ESP_IDF
#ifdef USE_WEBSERVER_OTA

#include <esp_http_server.h>
#include <multipart_parser.h>
#include <string>
#include <functional>

namespace esphome {
namespace web_server_idf {

// Wrapper around zorxx/multipart-parser for ESP-IDF OTA uploads
class MultipartReader {
 public:
  struct Part {
    std::string name;
    std::string filename;
    std::string content_type;
  };

  using DataCallback = std::function<void(const uint8_t *data, size_t len)>;
  using PartCompleteCallback = std::function<void()>;

  explicit MultipartReader(const std::string &boundary);
  ~MultipartReader();

  // Set callbacks for handling data
  void set_data_callback(DataCallback callback) { data_callback_ = callback; }
  void set_part_complete_callback(PartCompleteCallback callback) { part_complete_callback_ = callback; }

  // Parse incoming data
  size_t parse(const char *data, size_t len);

  // Get current part info
  const Part &get_current_part() const { return current_part_; }

  // Check if we found a file upload
  bool has_file() const { return !current_part_.filename.empty(); }

 private:
  static int on_header_field(multipart_parser *parser, const char *at, size_t length);
  static int on_header_value(multipart_parser *parser, const char *at, size_t length);
  static int on_part_data_begin(multipart_parser *parser);
  static int on_part_data(multipart_parser *parser, const char *at, size_t length);
  static int on_part_data_end(multipart_parser *parser);
  static int on_headers_complete(multipart_parser *parser);

  multipart_parser *parser_{nullptr};
  multipart_parser_settings settings_{};

  Part current_part_;
  std::string current_header_field_;
  std::string current_header_value_;

  DataCallback data_callback_;
  PartCompleteCallback part_complete_callback_;

  bool in_headers_{false};

  void process_header_();
};

}  // namespace web_server_idf
}  // namespace esphome
#endif  // USE_WEBSERVER_OTA
#endif  // USE_ESP_IDF