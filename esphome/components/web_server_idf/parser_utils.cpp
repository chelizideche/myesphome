#include "esphome/core/defines.h"
#ifdef USE_ESP_IDF
#include "parser_utils.h"
#include <cstring>
#include <cctype>

namespace esphome {
namespace web_server_idf {

// Helper function for case-insensitive string region comparison
bool str_ncmp_ci(const char *s1, const char *s2, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (!char_equals_ci(s1[i], s2[i])) {
      return false;
    }
  }
  return true;
}

// Case-insensitive string search (like strstr but case-insensitive)
const char *stristr(const char *haystack, const char *needle) {
  if (!haystack) {
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

}  // namespace web_server_idf
}  // namespace esphome
#endif  // USE_ESP_IDF
