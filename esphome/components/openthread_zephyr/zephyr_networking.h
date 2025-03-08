#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ZEPHYR_NETWORKING

// Include Zephyr networking headers
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_if.h>

// Define LwIP types to map to Zephyr types
typedef struct net_ipaddr {
  sa_family_t family;
  union {
    struct in_addr in4_addr;
    struct in6_addr in6_addr;
  };
} ip_addr_t;

typedef struct in_addr ip4_addr_t;
typedef struct in6_addr ip6_addr_t;

// Define LwIP functions to map to Zephyr functions
#define ip_addr_set_zero(addr) do { \
  if ((addr) != NULL) { \
    memset((addr), 0, sizeof(ip_addr_t)); \
    ((ip_addr_t*)(addr))->family = AF_INET; \
  } \
} while(0)

#define IP_ADDR4(addr, a, b, c, d) do { \
  if ((addr) != NULL) { \
    ((ip_addr_t*)(addr))->family = AF_INET; \
    ((ip_addr_t*)(addr))->in4_addr.s_addr = htonl(((a) << 24) | ((b) << 16) | ((c) << 8) | (d)); \
  } \
} while(0)

#define ip_addr_copy(dest, src) do { \
  if ((&(dest) != NULL) && (&(src) != NULL)) { \
    memcpy(&(dest), &(src), sizeof(ip_addr_t)); \
  } \
} while(0)

#define ipaddr_aton(addr_str, addr) inet_pton(AF_INET, (addr_str), &((ip_addr_t*)(addr))->in4_addr)

#define ip_addr_isany(addr) ((addr) == NULL || \
  (((ip_addr_t*)(addr))->family == AF_INET && ((ip_addr_t*)(addr))->in4_addr.s_addr == 0) || \
  (((ip_addr_t*)(addr))->family == AF_INET6 && \
   memcmp(&((ip_addr_t*)(addr))->in6_addr, &in6addr_any, sizeof(struct in6_addr)) == 0))

#define IP_IS_V4(addr) (((ip_addr_t*)(addr))->family == AF_INET)
#define IP_IS_V6(addr) (((ip_addr_t*)(addr))->family == AF_INET6)

#define ip_addr_ismulticast(addr) ( \
  (IP_IS_V4(addr) && (ntohl(((ip_addr_t*)(addr))->in4_addr.s_addr) & 0xF0000000) == 0xE0000000) || \
  (IP_IS_V6(addr) && (((ip_addr_t*)(addr))->in6_addr.s6_addr[0] == 0xFF)))

inline char* ipaddr_ntoa(const ip_addr_t* addr) {
  static char buf[INET6_ADDRSTRLEN];
  if (addr == NULL) return NULL;
  
  if (IP_IS_V4(addr)) {
    inet_ntop(AF_INET, &addr->in4_addr, buf, sizeof(buf));
  } else {
    inet_ntop(AF_INET6, &addr->in6_addr, buf, sizeof(buf));
  }
  return buf;
}

#define ip_addr_cmp(addr1, addr2) ( \
  ((addr1) != NULL && (addr2) != NULL) && \
  (((ip_addr_t*)(addr1))->family == ((ip_addr_t*)(addr2))->family) && \
  (((ip_addr_t*)(addr1))->family == AF_INET ? \
   (((ip_addr_t*)(addr1))->in4_addr.s_addr == ((ip_addr_t*)(addr2))->in4_addr.s_addr) : \
   (memcmp(&((ip_addr_t*)(addr1))->in6_addr, &((ip_addr_t*)(addr2))->in6_addr, sizeof(struct in6_addr)) == 0)))

#define ip_2_ip4(addr) (&((ip_addr_t*)(addr))->in4_addr)

#define ip_addr_get_ip4_u32(addr) (ntohl(((ip_addr_t*)(addr))->in4_addr.s_addr))

// Define u8_t for compatibility
typedef uint8_t u8_t;

#endif // USE_ZEPHYR_NETWORKING 