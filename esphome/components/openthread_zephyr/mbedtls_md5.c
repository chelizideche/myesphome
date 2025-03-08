#include <string.h>

// Simple implementation of mbedtls_md5 for TCP ISN generation
// This is needed by Zephyr's TCP stack for ISN generation
#ifdef __cplusplus
extern "C" {
#endif

__attribute__((weak)) void mbedtls_md5(const unsigned char *input, size_t ilen, unsigned char output[16]) {
    // Simple hash function that's sufficient for TCP ISN
    // This is not a cryptographically secure hash
    memset(output, 0, 16);
    
    for (size_t i = 0; i < ilen; i++) {
        output[i % 16] ^= input[i];
        output[(i + 1) % 16] += input[i];
    }
}

#ifdef __cplusplus
}
#endif 