#include "table.h"
#include "endian_util.h"
#include <string.h>

#ifndef TABLE_PAGE_SIZE
#define TABLE_PAGE_SIZE 4096
#endif


static int compute_capacity(int record_size) {
  if (record_size <= 0 || record_size > TABLE_PAGE_SIZE)
    return 0;

  size_t available = TABLE_PAGE_SIZE - TABLE_HDR_SIZE;

  if (available < record_size)
    return 0;

  size_t guess = available / record_size;

  if (guess <= 0)
    return 0;

  for (int c = guess; c > 0; c--) {
    size_t bitmap_bytes = (c + 7) / 8;

    size_t total = TABLE_HDR_SIZE + bitmap_bytes + (size_t) (c * record_size);

    if (total <= TABLE_PAGE_SIZE)
      return c;
  }
  return 0;
}

int tbl_init_leaf(void* page, uint16_t record_size) {
  if (!page || record_size != TABLE_RECORD_SIZE)
    return TABLE_E_INVAL;

  memset(page, 0, TABLE_PAGE_SIZE);

  uint16_t capacity = (uint16_t) compute_capacity(record_size);
  if (!capacity)
    return TABLE_E_LAYOUT;

  hdr_set_kind(page, TABLE_PAGE_KIND_LEAF);
  hdr_set_record_size(page, record_size);
  hdr_set_capacity(page, capacity);
  hdr_set_used_count(page, 0);
  hdr_set_next_page(page, 0);
  hdr_clear_reserved(page);

  return TABLE_OK;
}


// ─────────────────────────────────────────────────────────────────────────────
// Header accessors (internal)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Read the page kind (e.g. TABLE_PAGE_KIND_LEAF) from the header.
 */
static inline uint16_t hdr_kind(const void* page) {
  const uint8_t* base = (const uint8_t*)page;
  return read_le_u16(base + TABLE_HDR_KIND_OFF);
}

/**
 * @brief Write the page kind into the header.
 */
static inline void hdr_set_kind(void* page, uint16_t v) {
  uint8_t* base = (uint8_t*)page;
  write_le_u16(base + TABLE_HDR_KIND_OFF, v);
}

/**
 * @brief Read the record size field from the header.
 */
static inline uint16_t hdr_record_size(const void* page) {
  const uint8_t* base = (const uint8_t*)page;
  return read_le_u16(base + TABLE_HDR_RECORD_SIZE_OFF);
}

/**
 * @brief Write the record size field into the header.
 */
static inline void hdr_set_record_size(void* page, uint16_t v) {
  uint8_t* base = (uint8_t*)page;
  write_le_u16(base + TABLE_HDR_RECORD_SIZE_OFF, v);
}

/**
 * @brief Read the capacity (max number of records) from the header.
 */
static inline uint16_t hdr_capacity(const void* page) {
  const uint8_t* base = (const uint8_t*)page;
  return read_le_u16(base + TABLE_HDR_CAPACITY_OFF);
}

/**
 * @brief Write the capacity field into the header.
 */
static inline void hdr_set_capacity(void* page, uint16_t v) {
  uint8_t* base = (uint8_t*)page;
  write_le_u16(base + TABLE_HDR_CAPACITY_OFF, v);
}

/**
 * @brief Read the number of used records from the header.
 */
static inline uint16_t hdr_used_count(const void* page) {
  const uint8_t* base = (const uint8_t*)page;
  return read_le_u16(base + TABLE_HDR_USED_COUNT_OFF);
}

/**
 * @brief Write the number of used records into the header.
 */
static inline void hdr_set_used_count(void* page, uint16_t v) {
  uint8_t* base = (uint8_t*)page;
  write_le_u16(base + TABLE_HDR_USED_COUNT_OFF, v);
}

/**
 * @brief Read the next page ID from the header.
 */
static inline uint32_t hdr_next_page(const void* page) {
  const uint8_t* base = (const uint8_t*)page;
  return read_le_u32(base + TABLE_HDR_NEXT_PAGE_OFF);
}

/**
 * @brief Write the next page ID into the header.
 */
static inline void hdr_set_next_page(void* page, uint32_t v) {
  uint8_t* base = (uint8_t*)page;
  write_le_u32(base + TABLE_HDR_NEXT_PAGE_OFF, v);
}

/**
 * @brief Zero out all reserved header fields.
 */
static inline void hdr_clear_reserved(void* page) {
  uint8_t* base = (uint8_t*)page;
  write_le_u32(base + TABLE_HDR_RESERVED0_OFF, 0);
  write_le_u32(base + TABLE_HDR_RESERVED1_OFF, 0);
  write_le_u32(base + TABLE_HDR_RESERVED2_OFF, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Bitmap & data helpers (internal)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Return a pointer to the start of the bitmap region.
 *
 * The bitmap directly follows the 24-byte header and contains one bit per slot.
 * Bit = 1 means the slot is used, 0 means free.
 */
static inline uint8_t* bitmap_ptr(void* page) {
  uint8_t* base = (uint8_t*)page;
  return base + TABLE_HDR_SIZE;
}

/**
 * @brief Return a const pointer to the start of the bitmap (read-only version).
 */
static inline const uint8_t* bitmap_ptr_c(const void* page) {
  const uint8_t* base = (const uint8_t*)page;
  return base + TABLE_HDR_SIZE;
}

/**
 * @brief Compute the size of the bitmap in bytes for a given capacity.
 *
 * Each slot requires one bit; the size in bytes is the integer ceil(C/8),
 * implemented as (C + 7) / 8.
 */
static inline size_t bitmap_size_bytes(uint16_t capacity) {
  return (capacity + 7u) / 8u;
}

/**
 * @brief Compute the byte offset where record data begins.
 */
static inline size_t data_offset(uint16_t capacity) {
  return TABLE_HDR_SIZE + bitmap_size_bytes(capacity);
}

/**
 * @brief Return a pointer to the first record slot (data area).
 */
static inline uint8_t* data_ptr(void* page, uint16_t capacity) {
  uint8_t* base = (uint8_t*)page;
  return base + data_offset(capacity);
}

/**
 * @brief Return a const pointer to the first record slot (read-only).
 */
static inline const uint8_t* data_ptr_c(const void* page, uint16_t capacity) {
  const uint8_t* base = (const uint8_t*)page;
  return base + data_offset(capacity);
}
