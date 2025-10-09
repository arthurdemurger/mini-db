#ifndef ENDIAN_UTIL_H
#define ENDIAN_UTIL_H

#include <stdint.h>

/**
 * @brief Read a 16-bit unsigned integer stored in little-endian order.
 * @param p Pointer to the first byte.
 * @return The 16-bit integer in host byte order.
 */
static inline uint16_t read_le_u16(const uint8_t* p) {
  return (uint16_t)p[0]
       | ((uint16_t)p[1] << 8);
}

/**
 * @brief Read a 32-bit unsigned integer stored in little-endian order.
 * @param p Pointer to the first byte.
 * @return The 32-bit integer in host byte order.
 */
static inline uint32_t read_le_u32(const uint8_t* p) {
  return (uint32_t)p[0]
       | ((uint32_t)p[1] << 8)
       | ((uint32_t)p[2] << 16)
       | ((uint32_t)p[3] << 24);
}

/**
 * @brief Write a 16-bit unsigned integer to memory in little-endian order.
 * @param p Pointer to destination buffer (at least 2 bytes).
 * @param v The 16-bit integer to write.
 */
static inline void write_le_u16(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFFu);
  p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

/**
 * @brief Write a 32-bit unsigned integer to memory in little-endian order.
 * @param p Pointer to destination buffer (at least 4 bytes).
 * @param v The 32-bit integer to write.
 */
static inline void write_le_u32(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xFFu);
  p[1] = (uint8_t)((v >> 8) & 0xFFu);
  p[2] = (uint8_t)((v >> 16) & 0xFFu);
  p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

#endif /* ENDIAN_UTIL_H */
