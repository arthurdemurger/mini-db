#ifndef PAGER_H

#define PAGER_H

// ─────────────────────────────────────────────────────────────────────────────
// Libraries
// ─────────────────────────────────────────────────────────────────────────────
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

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
 * @brief Write one full page from memory to disk (Design A).
 *
 * Writes exactly page_size bytes at offset page_no * page_size.
 * The pager must have been opened in read/write mode.
 *
 * @param[in] p         Pager handle (non-null).
 * @param[in] page_no   Page index (0-based, must be < page_count).
 * @param[in] page_buf  Pointer to a buffer of size page_size.
 * @return PAGER_OK on success,
 *         PAGER_E_INVAL for bad args,
 *         PAGER_E_RANGE if page_no >= page_count,
 *         PAGER_E_IO on I/O failure,
 *         PAGER_E_META if offset overflow detected.
 */
int pager_write(const Pager* p, uint32_t page_no, const void* page_buf);

/**
 * @brief Allocate a new blank page at the end of the file
 *
 * - Extends the file by one page filled with zeros.
 * - Updates the in-memory and on-disk page_count.
 * - Returns the new page number via out_page_no.
 *
 * @param[in,out] p           Pager handle (opened read/write).
 * @param[out] out_page_no    Receives index of the newly allocated page.
 * @return PAGER_OK on success or a negative PagerError on failure.
 */
int pager_alloc_page(Pager* p, uint32_t* out_page_no);

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