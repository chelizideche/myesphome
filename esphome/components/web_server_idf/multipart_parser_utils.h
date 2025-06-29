#pragma once
#ifdef USE_ESP_IDF
#ifdef USE_WEBSERVER_OTA

#include <string>
#include <cctype>
#include <cstring>

namespace esphome {
namespace web_server_idf {

// Helper function for case-insensitive character comparison
inline bool char_equals_ci(char a, char b) { return ::tolower(a) == ::tolower(b); }

// Helper function for case-insensitive string region comparison
inline bool str_ncmp_ci(const char *s1, const char *s2, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (!char_equals_ci(s1[i], s2[i])) {
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
  return str_ncmp_ci(str.c_str(), prefix.c_str(), prefix.length());
}

// Find a substring case-insensitively
inline size_t str_find_case_insensitive(const std::string &haystack, const std::string &needle, size_t pos = 0) {
  if (needle.empty() || pos >= haystack.length()) {
    return std::string::npos;
  }

  const size_t needle_len = needle.length();
  const size_t max_pos = haystack.length() - needle_len;

  for (size_t i = pos; i <= max_pos; i++) {
    if (str_ncmp_ci(haystack.c_str() + i, needle.c_str(), needle_len)) {
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

// Case-insensitive string search (like strstr but case-insensitive)
inline const char *stristr(const char *haystack, const char *needle) {
  if (!haystack || !needle) {
    return nullptr;
  }

  size_t needle_len = strlen(needle);
  if (needle_len == 0) {
    return haystack;
  }

  for (const char *p = haystack; *p; p++) {
    if (str_ncmp_ci(p, needle, needle_len)) {
      return p;
    }
  }

  return nullptr;
}

// Parse boundary from Content-Type header
// Returns true if boundary found, false otherwise
// boundary_start and boundary_len will point to the boundary value
inline bool parse_multipart_boundary(const char *content_type, const char **boundary_start, size_t *boundary_len) {
  if (!content_type) {
    return false;
  }

  // Check for multipart/form-data (case-insensitive)
  if (!stristr(content_type, "multipart/form-data")) {
    return false;
  }

  // Look for boundary parameter
  const char *b = stristr(content_type, "boundary=");
  if (!b) {
    return false;
  }

  const char *start = b + 9;  // Skip "boundary="

  // Skip whitespace
  while (*start == ' ' || *start == '\t') {
    start++;
  }

  if (!*start) {
    return false;
  }

  // Find end of boundary
  const char *end = start;
  if (*end == '"') {
    // Quoted boundary
    start++;
    end++;
    while (*end && *end != '"') {
      end++;
    }
    *boundary_len = end - start;
  } else {
    // Unquoted boundary
    while (*end && *end != ' ' && *end != ';' && *end != '\r' && *end != '\n' && *end != '\t') {
      end++;
    }
    *boundary_len = end - start;
  }

  if (*boundary_len == 0) {
    return false;
  }

  *boundary_start = start;
  return true;
}

// Check if content type is form-urlencoded (case-insensitive)
inline bool is_form_urlencoded(const char *content_type) {
  if (!content_type) {
    return false;
  }

  return stristr(content_type, "application/x-www-form-urlencoded") != nullptr;
}

// Trim whitespace from both ends of a string
inline std::string str_trim(const std::string &str) {
  size_t start = str.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return "";
  }
  size_t end = str.find_last_not_of(" \t\r\n");
  return str.substr(start, end - start + 1);
}

// Extract header value (everything after the colon)
inline std::string extract_header_value(const std::string &header) {
  size_t colon_pos = header.find(':');
  if (colon_pos == std::string::npos) {
    return "";
  }
  return str_trim(header.substr(colon_pos + 1));
}

}  // namespace web_server_idf
}  // namespace esphome
#endif  // USE_WEBSERVER_OTA
#endif  // USE_ESP_IDF
