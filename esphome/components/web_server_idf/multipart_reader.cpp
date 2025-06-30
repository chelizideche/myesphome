#include "esphome/core/defines.h"
#if defined(USE_ESP_IDF) && defined(USE_WEBSERVER_OTA)
#include "multipart_reader.h"
#include "multipart_parser_utils.h"
#include "esphome/core/log.h"
#include <cstring>
#include "multipart_parser.h"

namespace esphome {
namespace web_server_idf {

static const char *const TAG = "multipart_reader";

MultipartReader::MultipartReader(const std::string &boundary) {
  // Initialize settings with callbacks
  memset(&settings_, 0, sizeof(settings_));
  settings_.on_header_field = on_header_field;
  settings_.on_header_value = on_header_value;
  settings_.on_part_data_begin = on_part_data_begin;
  settings_.on_part_data = on_part_data;
  settings_.on_part_data_end = on_part_data_end;
  settings_.on_headers_complete = on_headers_complete;

  ESP_LOGV(TAG, "Initializing multipart parser with boundary: '%s' (len: %zu)", boundary.c_str(), boundary.length());

  // Create parser with boundary
  parser_ = multipart_parser_init(boundary.c_str(), &settings_);
  if (parser_) {
    multipart_parser_set_data(parser_, this);
  } else {
    ESP_LOGE(TAG, "Failed to initialize multipart parser");
  }
}

MultipartReader::~MultipartReader() {
  if (parser_) {
    multipart_parser_free(parser_);
  }
}

size_t MultipartReader::parse(const char *data, size_t len) {
  if (!parser_) {
    ESP_LOGE(TAG, "Parser not initialized");
    return 0;
  }

  size_t parsed = multipart_parser_execute(parser_, data, len);

  if (parsed != len) {
    ESP_LOGW(TAG, "Parser consumed %zu of %zu bytes - possible error", parsed, len);
  }

  return parsed;
}

void MultipartReader::process_header_() {
  if (str_startswith_case_insensitive(current_header_field_, "content-disposition")) {
    // Parse name and filename from Content-Disposition
    current_part_.name = extract_header_param(current_header_value_, "name");
    current_part_.filename = extract_header_param(current_header_value_, "filename");
  } else if (str_startswith_case_insensitive(current_header_field_, "content-type")) {
    current_part_.content_type = str_trim(current_header_value_);
  }
}

int MultipartReader::on_header_field(multipart_parser *parser, const char *at, size_t length) {
  MultipartReader *reader = static_cast<MultipartReader *>(multipart_parser_get_data(parser));

  // If we were processing a value, save it
  if (!reader->current_header_value_.empty()) {
    reader->process_header_();
    reader->current_header_value_.clear();
  }

  // Start new header field
  reader->current_header_field_.assign(at, length);
  reader->in_headers_ = true;

  return 0;
}

int MultipartReader::on_header_value(multipart_parser *parser, const char *at, size_t length) {
  MultipartReader *reader = static_cast<MultipartReader *>(multipart_parser_get_data(parser));
  reader->current_header_value_.append(at, length);
  return 0;
}

int MultipartReader::on_headers_complete(multipart_parser *parser) {
  MultipartReader *reader = static_cast<MultipartReader *>(multipart_parser_get_data(parser));

  // Process last header if any
  if (!reader->current_header_value_.empty()) {
    reader->process_header_();
  }

  reader->in_headers_ = false;
  reader->current_header_field_.clear();
  reader->current_header_value_.clear();

  ESP_LOGV(TAG, "Part headers complete: name='%s', filename='%s', content_type='%s'",
           reader->current_part_.name.c_str(), reader->current_part_.filename.c_str(),
           reader->current_part_.content_type.c_str());

  return 0;
}

int MultipartReader::on_part_data_begin(multipart_parser *parser) {
  MultipartReader *reader = static_cast<MultipartReader *>(multipart_parser_get_data(parser));
  ESP_LOGV(TAG, "Part data begin");
  return 0;
}

int MultipartReader::on_part_data(multipart_parser *parser, const char *at, size_t length) {
  MultipartReader *reader = static_cast<MultipartReader *>(multipart_parser_get_data(parser));

  // Only process file uploads
  if (reader->has_file() && reader->data_callback_) {
    // IMPORTANT: The 'at' pointer points to data within the parser's input buffer.
    // This data is only valid during this callback. The callback handler MUST
    // process or copy the data immediately - it cannot store the pointer for
    // later use as the buffer will be overwritten.
    reader->data_callback_(reinterpret_cast<const uint8_t *>(at), length);
  }

  return 0;
}

int MultipartReader::on_part_data_end(multipart_parser *parser) {
  MultipartReader *reader = static_cast<MultipartReader *>(multipart_parser_get_data(parser));

  ESP_LOGV(TAG, "Part data end");

  if (reader->part_complete_callback_) {
    reader->part_complete_callback_();
  }

  // Clear part info for next part
  reader->current_part_ = Part{};

  return 0;
}

}  // namespace web_server_idf
}  // namespace esphome
#endif  // defined(USE_ESP_IDF) && defined(USE_WEBSERVER_OTA)
