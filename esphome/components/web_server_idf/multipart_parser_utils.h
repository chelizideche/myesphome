#pragma once
#ifdef USE_ESP_IDF
#ifdef USE_WEBSERVER_OTA

#include <string>
#include <cctype>

namespace esphome {
namespace web_server_idf {

// Case-insensitive string comparison
inline bool str_equals_case_insensitive(const std::string &a, const std::string &b) {
  if (a.length() != b.length()) {
    return false;
  }
  for (size_t i = 0; i < a.length(); i++) {
    if (tolower(a[i]) != tolower(b[i])) {
      return false;
    }
  }
  return true;
}

// Case-insensitive string prefix check
inline bool str_startswith_case_insensitive(const std::string &str, const std::string &prefix) {
  if (str.length() < prefix.length()) {
    return false;
  }
  for (size_t i = 0; i < prefix.length(); i++) {
    if (tolower(str[i]) != tolower(prefix[i])) {
      return false;
    }
  }
  return true;
}

// Find a substring case-insensitively
inline size_t str_find_case_insensitive(const std::string &haystack, const std::string &needle, size_t pos = 0) {
  if (needle.empty() || pos >= haystack.length()) {
    return std::string::npos;
  }

  for (size_t i = pos; i <= haystack.length() - needle.length(); i++) {
    bool match = true;
    for (size_t j = 0; j < needle.length(); j++) {
      if (tolower(haystack[i + j]) != tolower(needle[j])) {
        match = false;
        break;
      }
    }
    if (match) {
      return i;
    }
  }

  return std::string::npos;
}

// Extract a parameter value from a header line
// Handles both quoted and unquoted values
inline std::string extract_header_param(const std::string &header, const std::string &param) {
  size_t search_pos = 0;

  while (search_pos < header.length()) {
    // Look for param name
    size_t pos = str_find_case_insensitive(header, param, search_pos);
    if (pos == std::string::npos) {
      return "";
    }

    // Check if this is a word boundary (not part of another parameter)
    if (pos > 0 && header[pos - 1] != ' ' && header[pos - 1] != ';' && header[pos - 1] != '\t') {
      search_pos = pos + 1;
      continue;
    }

    // Move past param name
    pos += param.length();

    // Skip whitespace and find '='
    while (pos < header.length() && (header[pos] == ' ' || header[pos] == '\t')) {
      pos++;
    }

    if (pos >= header.length() || header[pos] != '=') {
      search_pos = pos;
      continue;
    }

    pos++;  // Skip '='

    // Skip whitespace after '='
    while (pos < header.length() && (header[pos] == ' ' || header[pos] == '\t')) {
      pos++;
    }

    if (pos >= header.length()) {
      return "";
    }

    // Check if value is quoted
    if (header[pos] == '"') {
      pos++;
      size_t end = header.find('"', pos);
      if (end != std::string::npos) {
        return header.substr(pos, end - pos);
      }
      // Malformed - no closing quote
      return "";
    }

    // Unquoted value - find the end (semicolon, comma, or end of string)
    size_t end = pos;
    while (end < header.length() && header[end] != ';' && header[end] != ',' && header[end] != ' ' &&
           header[end] != '\t') {
      end++;
    }

    return header.substr(pos, end - pos);
  }

  return "";
}

}  // namespace web_server_idf
}  // namespace esphome
#endif  // USE_WEBSERVER_OTA
#endif  // USE_ESP_IDF