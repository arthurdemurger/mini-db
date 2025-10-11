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
 * @brief Validate the internal consistency of a TABLE_LEAF page.
 * Checks header fields, recomputed capacity, used_count bounds, bitmap popcount
 * equality, geometry (header + bitmap + data fits in page), and that high bits
 * beyond capacity in the last bitmap byte are zero (LSB-first layout).
 * @param[in] page Non-null pointer to a 4 KiB page.
 * @return TABLE_OK on success,
 *         TABLE_E_INVAL (bad args),
 *         TABLE_E_BADKIND (wrong kind),
 *         TABLE_E_LAYOUT (record_size/capacity/geometry issues),
 *         TABLE_E_BITMAP (bitmap popcount or extra bits set).
 */
int   tbl_validate(const void* page);

/**
 * @brief Find the first free slot (bit = 0) scanning LSB-first.
 * Does not modify the page; relies only on the bitmap. The caller may
 * subsequently call tbl_slot_mark_used(idx) to reserve that slot.
 * @param[in] page Non-null pointer to a TABLE_LEAF page.
 * @return Slot index in [0..capacity-1] if a free slot exists, or -1 if none.
 */
int   tbl_slot_find_free(const void* page);

/**
 * @brief Mark a slot as used (set bit = 1) and increment used_count.
 * Requires: 0 <= idx < capacity; slot must currently be free (bit = 0).
 * @param[in,out] page Page buffer to modify.
 * @param[in] idx Slot index.
 * @return TABLE_OK on success,
 *         TABLE_E_INVAL if page/idx are invalid or slot already used,
 *         TABLE_E_FULL if used_count == capacity,
 *         TABLE_E_LAYOUT if used_count > capacity (corruption).
 */
int   tbl_slot_mark_used(void* page, int idx);

/**
 * @brief Mark a slot as free (set bit = 0) and decrement used_count.
 * Requires: 0 <= idx < capacity; slot must currently be used (bit = 1).
 * @param[in,out] page Page buffer to modify.
 * @param[in] idx Slot index.
 * @return TABLE_OK on success,
 *         TABLE_E_INVAL if page/idx are invalid, slot already free, or used_count == 0,
 *         TABLE_E_LAYOUT if used_count > capacity (corruption).
 */
int   tbl_slot_mark_free(void* page, int idx);

/**
 * @brief Return a pointer to record slot idx inside the page's data area.
 * No bitmap/state checks are performed; the caller must ensure the slot state.
 * @param[in,out] page Page buffer.
 * @param[in] idx Slot index; must satisfy 0 <= idx < capacity.
 * @return Non-NULL pointer to record memory, or NULL if out of range.
 */
void* tbl_slot_ptr(void* page, int idx);

/**
 * @brief Const-qualified variant of tbl_slot_ptr.
 * @see tbl_slot_ptr
 */
const void* tbl_slot_ptr_c(const void* page, int idx);

/**
 * @brief Getters for header fields (read-only).
 * Return the raw header values; the page is expected to be validated by caller.
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