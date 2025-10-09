#ifndef TABLE_H

#define TABLE_H

// ─────────────────────────────────────────────────────────────────────────────
// Libraries
// ─────────────────────────────────────────────────────────────────────────────
#include <stdint.h>

// ─────────────────────────────────────────────────────────────────────────────
/* Public constants (V1)
 * - Page type: TABLE_PAGE_KIND_LEAF (0x0001)
 * - Record size fixed at 128 B (TABLE_RECORD_SIZE)
 * - Header is 24 B; all multi-byte integers are little-endian on disk.
 */
// ─────────────────────────────────────────────────────────────────────────────
#define TABLE_PAGE_KIND_LEAF        0x0001
#define TABLE_RECORD_SIZE           128
#define TABLE_HDR_SIZE              24

/* Header offsets (bytes) */
#define TABLE_HDR_KIND_OFF          0   /* u16 */
#define TABLE_HDR_RECORD_SIZE_OFF   2   /* u16 (V1 = 128) */
#define TABLE_HDR_CAPACITY_OFF      4   /* u16 */
#define TABLE_HDR_USED_COUNT_OFF    6   /* u16 */
#define TABLE_HDR_NEXT_PAGE_OFF     8   /* u32 */
#define TABLE_HDR_RESERVED0_OFF     12  /* u32 */
#define TABLE_HDR_RESERVED1_OFF     16  /* u32 */
#define TABLE_HDR_RESERVED2_OFF     20  /* u32 */

/* Bitmap: placed immediately after header; size = ceil(capacity/8).
 * Bit ordering: LSB-first within each byte (bit 0 => slot 0).
 */

// ─────────────────────────────────────────────────────────────────────────────
// Error codes (public)
// ─────────────────────────────────────────────────────────────────────────────
typedef enum {
  TABLE_OK = 0,
  TABLE_E_INVAL = -1,
  TABLE_E_BADKIND = -2,
  TABLE_E_LAYOUT = -3,
  TABLE_E_BITMAP = -4,
  TABLE_E_FULL = -5
} TableError;


// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Initialize a TABLE_LEAF page in memory.
 *
 * Sets header fields, computes capacity, clears bitmap, and resets counters.
 * The record size must be 128 bytes in V1.
 *
 * @param[in,out] page         Pointer to a 4 KiB page buffer.
 * @param[in]     record_size  Record size (must be 128 for V1).
 * @return TABLE_OK on success or a negative TableError on failure.
 */
int   tbl_init_leaf(void* page, uint16_t record_size /*=128*/);


/**
 * @brief Validate invariants for a TABLE_LEAF page.
 *
 * Checks header values, record size, capacity formula, used count,
 * bitmap consistency, and that no bits beyond capacity are set.
 *
 * @param[in] page  Pointer to a 4 KiB page buffer.
 * @return TABLE_OK if valid, or a negative TableError if corrupted.
 */
int   tbl_validate(const void* page);

/**
 * @brief Find the first free record slot in a TABLE_LEAF page.
 *
 * @param[in] page  Pointer to a 4 KiB page buffer.
 * @return The slot index (>=0) if available, or -1 if the page is full.
 */
int   tbl_slot_find_free(const void* page);

/**
 * @brief Mark a record slot as used and increment the used count.
 *
 * @param[in,out] page  Pointer to the page buffer.
 * @param[in]     idx   Slot index to mark.
 * @return TABLE_OK on success, or TABLE_E_INVAL if index is invalid.
 */
int   tbl_slot_mark_used(void* page, int idx);

/**
 * @brief Mark a record slot as free and decrement the used count.
 *
 * @param[in,out] page  Pointer to the page buffer.
 * @param[in]     idx   Slot index to clear.
 * @return TABLE_OK on success, or TABLE_E_INVAL if index is invalid.
 */
int   tbl_slot_mark_free(void* page, int idx);

/**
 * @brief Return a pointer to the record at the specified slot.
 *
 * The caller must ensure the slot index is within [0, capacity).
 *
 * @param[in,out] page  Pointer to the page buffer.
 * @param[in]     idx   Slot index.
 * @return Pointer to record data within the page, or NULL if invalid.
 */
void* tbl_slot_ptr(void* page, int idx);

/**
 * @brief Retrieve the capacity (number of record slots) of the page.
 */
uint16_t tbl_get_capacity(const void* page);

/**
 * @brief Retrieve the record size in bytes.
 */
uint16_t tbl_get_record_size(const void* page);

/**
 * @brief Retrieve the current number of used slots.
 */
uint16_t tbl_get_used_count(const void* page);

/**
 * @brief Retrieve the next-page number (0 if none).
 */
uint32_t tbl_get_next_page(const void* page);

#endif //TABLE_H