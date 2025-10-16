#include "table.h"
#include "endian_util.h"
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// Defines
// ─────────────────────────────────────────────────────────────────────────────
#ifndef TABLE_PAGE_SIZE
#define TABLE_PAGE_SIZE 4096
#endif

// ───────────── Header accessors ─────────────
static inline uint16_t hdr_kind(const void* page);
static inline void hdr_set_kind(void* page, uint16_t v);
static inline uint16_t hdr_record_size(const void* page);
static inline void hdr_set_record_size(void* page, uint16_t v);
static inline uint16_t hdr_capacity(const void* page);
static inline void hdr_set_capacity(void* page, uint16_t v);
static inline uint16_t hdr_used_count(const void* page);
static inline void hdr_set_used_count(void* page, uint16_t v);
static inline uint32_t hdr_next_page(const void* page);
static inline void hdr_set_next_page(void* page, uint32_t v);
static inline void hdr_clear_reserved(void* page);

// ───────────── Bitmap helpers ─────────────
static inline uint8_t* bitmap_ptr(void* page);
static inline const uint8_t* bitmap_ptr_c(const void* page);
static inline size_t bitmap_size_bytes(uint16_t capacity);
static inline size_t bitmap_popcount(const uint8_t* bm, size_t nbytes);

// ───────────── Data helpers ─────────────
static inline size_t data_offset(uint16_t capacity);
static int compute_capacity(int record_size);

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

int tbl_validate(const void* page) {
  if (!page)
    return TABLE_E_INVAL;

  if (hdr_kind(page) != TABLE_PAGE_KIND_LEAF)
    return TABLE_E_BADKIND;

  uint16_t record_size = hdr_record_size(page);
  if (record_size != TABLE_RECORD_SIZE)
    return TABLE_E_LAYOUT;

  uint16_t cap = hdr_capacity(page);

  if (cap < 1 || cap != compute_capacity(record_size))
    return TABLE_E_LAYOUT;

  uint16_t used = hdr_used_count(page);
  if (used > cap)
    return TABLE_E_LAYOUT;

  size_t bm_bytes = bitmap_size_bytes(cap);

  size_t bm_pop = bitmap_popcount(bitmap_ptr_c(page), bm_bytes);
  if (bm_pop != used)
    return TABLE_E_BITMAP;

  size_t total = data_offset(cap) + (size_t)(cap * record_size);
  if (total > TABLE_PAGE_SIZE)
    return TABLE_E_LAYOUT;

  uint16_t valid_bits_last = (uint16_t)(cap & 7u);
  if (valid_bits_last != 0) {
    const uint8_t* bm = bitmap_ptr_c(page);
    uint8_t last_byte = bm[bm_bytes - 1];
    uint8_t invalid_mask = (uint8_t)(0xFFu << valid_bits_last);
    if ((last_byte & invalid_mask) != 0)
      return TABLE_E_BITMAP;
  }
  return TABLE_OK;
}

int tbl_slot_find_free(const void* page) {
  if (!page)
    return -1;

  uint16_t cap = hdr_capacity(page);
  uint16_t used = hdr_used_count(page);

  if (cap == 0 || used == cap)
    return -1;

  const uint8_t *bm = bitmap_ptr_c(page);
  size_t bm_bytes = bitmap_size_bytes(cap);

  for (size_t i = 0; i < bm_bytes; i++) {
    if (bm[i] != 0xFF) {
      for (int j = 0; j < 8; j++) {
        if ((bm[i] & (1u << j)) == 0) {
          size_t idx = i * 8 + j;
          if (idx < cap) {
            return idx;
          }
        }
      }
    }
  }
  return -1;
}

int tbl_slot_mark_used(void* page, int idx) {
  if (!page || idx < 0)
    return TABLE_E_INVAL;

  uint16_t cap = hdr_capacity(page);

  if (cap <= idx)
    return TABLE_E_INVAL;

  uint16_t used = hdr_used_count(page);
  if (used > cap)
    return TABLE_E_LAYOUT;

  if (used == cap)
    return TABLE_E_FULL;

  uint8_t *bm = bitmap_ptr(page);
  size_t byte = idx / 8;
  size_t bit = idx % 8;

  uint8_t bit_mask = (uint8_t) (1u << bit);
  if ((bm[byte] & bit_mask) != 0)
    return TABLE_E_INVAL;

  bm[byte] |= bit_mask;

  hdr_set_used_count(page, used + 1);
  return TABLE_OK;
}

int   tbl_slot_mark_free(void* page, int idx) {
  if (!page || idx < 0)
    return TABLE_E_INVAL;

  uint16_t cap = hdr_capacity(page);

  if (cap <= idx)
    return TABLE_E_INVAL;

  uint16_t used = hdr_used_count(page);
  if (used > cap)
    return TABLE_E_LAYOUT;

  if (used == 0)
    return TABLE_E_INVAL;

  uint8_t *bm = bitmap_ptr(page);
  size_t byte = idx / 8;
  size_t bit = idx % 8;

  uint8_t bit_mask = (uint8_t) (1u << bit);
  if ((bm[byte] & bit_mask) == 0)
    return TABLE_E_INVAL;

  bm[byte] &= (uint8_t) ~bit_mask;

  hdr_set_used_count(page, used - 1);
  return TABLE_OK;
}

void* tbl_slot_ptr(void* page, int idx){
  if (!page || idx < 0)
    return NULL;

  uint16_t cap = hdr_capacity(page);

  if (cap <= idx)
    return NULL;

  uint8_t* base = (uint8_t*)page;

  size_t offset = data_offset(cap);
  uint16_t record_size = hdr_record_size(page);

  return base + offset + (size_t) (record_size * idx);
}

const void* tbl_slot_ptr_c(const void* page, int idx) {
    if (!page || idx < 0)
    return NULL;

  uint16_t cap = hdr_capacity(page);

  if (cap <= idx)
    return NULL;

  const uint8_t* base = (const uint8_t*)page;

  size_t offset = data_offset(cap);
  uint16_t record_size = hdr_record_size(page);

  return base + offset + (size_t) (record_size * idx);
}

uint16_t tbl_get_kind(const void* page){
  return hdr_kind(page);
}

uint16_t tbl_get_capacity(const void* page){
  return hdr_capacity(page);
}

uint16_t tbl_get_record_size(const void* page){
  return hdr_record_size(page);
}

uint16_t tbl_get_used_count(const void* page){
  return hdr_used_count(page);
}

uint32_t tbl_get_next_page(const void* page){
  return hdr_next_page(page);
}

void tbl_set_next_page(void* page, uint32_t next_page) {
  hdr_set_next_page(page, next_page);
}

int tbl_slot_is_used(const void* page, int idx) {
  if (!page || idx < 0)
    return 0;

  uint16_t cap = hdr_capacity(page);

  if (cap <= idx)
    return 0;

  const uint8_t *bm = bitmap_ptr_c(page);
  size_t byte = idx / 8;
  size_t bit = idx % 8;

  uint8_t bit_mask = (uint8_t) (1u << bit);
  return (bm[byte] & bit_mask) != 0;
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
 * @brief Count the number of bits set in the bitmap (i.e. used slots).
 */
static inline size_t bitmap_popcount(const uint8_t* bm, size_t nbytes) {
  size_t count = 0;
  for (size_t i = 0; i < nbytes; i++) {
    uint8_t byte = bm[i];
    while (byte) {
      count += byte & 1u;
      byte >>= 1;
    }
  }
  return count;
}

/**
 * @brief Compute the byte offset where record data begins.
 */
static inline size_t data_offset(uint16_t capacity) {
  return TABLE_HDR_SIZE + bitmap_size_bytes(capacity);
}

static int compute_capacity(int record_size) {
  if (record_size <= 0 || record_size > TABLE_PAGE_SIZE)
    return 0;

  size_t available = TABLE_PAGE_SIZE - TABLE_HDR_SIZE;

  if (available < (size_t) record_size)
    return 0;

  size_t guess = available / record_size;

  if (guess <= 0)
    return 0;

  for (size_t c = guess; c > 0; c--) {
    size_t bitmap_bytes = (c + 7) / 8;

    size_t total = TABLE_HDR_SIZE + bitmap_bytes + (size_t) (c * record_size);

    if (total <= TABLE_PAGE_SIZE)
      return c;
  }
  return 0;
}
