#ifndef PAGER_H

#define PAGER_H

// ─────────────────────────────────────────────────────────────────────────────
// Libraries
// ─────────────────────────────────────────────────────────────────────────────
#include <stdint.h>
#include <stddef.h>

// ─────────────────────────────────────────────────────────────────────────────
// Error codes (public)
// ─────────────────────────────────────────────────────────────────────────────
typedef enum PagerError {
  PAGER_OK = 0,
  PAGER_E_IO = -1,
  PAGER_E_MAGIC = -2,
  PAGER_E_VERSION = -3,
  PAGER_E_PAGESIZE = -4,
  PAGER_E_META = -5,
  PAGER_E_TRUNCATED = -6,
  PAGER_E_RANGE = -7,
  PAGER_E_INVAL = -8
} PagerError;

// ─────────────────────────────────────────────────────────────────────────────
// On-disk format constants (v1)
// ─────────────────────────────────────────────────────────────────────────────
// Page 0 = header (20 bytes):
//   magic[0..3] = "MDB1"
//   version[4..7] = 1
//   page_size[8..11] = 4096
//   page_count[12..15] >= 1
//   flags[16..19] = 0
enum {
  PAGER_PAGE_SIZE = 4096,
  PAGER_HDR_SIZE  = 20
};

// ─────────────────────────────────────────────────────────────────────────────
// Public types
// ─────────────────────────────────────────────────────────────────────────────
typedef struct Pager Pager;

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Open a MiniDB file and validate its header.
 * @param path Path to the database file.
 * @param out  Output pointer to receive an allocated Pager* on success.
 * @return PAGER_OK or a negative PagerError code.
 */
int         pager_open(const char* path, Pager** out);

/**
 * @brief Close and free the Pager structure.
 */
void        pager_close(Pager* p);

/**
 * @brief Read a page from the file.
 * @param p          Pager instance.
 * @param page_no    Page number (0-based).
 * @param out_page_buf Destination buffer (must be at least page_size bytes).
 * @return PAGER_OK or a negative PagerError code.
 */
int         pager_read(const Pager* p, uint32_t page_no, void* out_page_buf);

/**
 * @brief Retrieve page geometry information.
 */
size_t      pager_page_size(const Pager* p);
uint32_t    pager_page_count(const Pager* p);

// ─────────────────────────────────────────────────────────────────────────────
// Utilities
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Return a human-readable string for a PagerError code.
 *        Useful for CLI, logs, or tests.
 */
const char* pager_errstr(int code);

#endif // PAGER_H