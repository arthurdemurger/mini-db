#ifndef TABLE_MANAGER_H
#define TABLE_MANAGER_H

#include <stdint.h>
#include "pager.h"
#include "table.h"

/**
 * @brief Initialize (or open-idempotent) the first leaf page of a table.
 *
 * Policy:
 * - Page 0 is reserved for the file header and MUST NOT be used by tables.
 * - If first_page_num >= pager_page_count, missing pages are allocated until
 *   first_page_num exists (Design A: explicit alloc via pager_alloc_page).
 * - If the target page is all zeros, it is initialized as a V1 leaf:
 *     kind = TABLE_PAGE_KIND_LEAF (0x0001),
 *     record_size = 128 (TABLE_RECORD_SIZE),
 *     used_count = 0,
 *     next_page = 0,
 *     bitmap cleared.
 * - If the target page already contains a VALID leaf page and is EMPTY
 *   (validate OK, record_size == 128, used_count == 0, next_page == 0),
 *   the operation is idempotent and returns TABLE_OK without rewriting.
 * - Otherwise (non-zero and not a valid empty leaf), the function refuses to
 *   overwrite and returns an error (e.g., TABLE_E_INVAL or TABLE_E_LAYOUT).
 *
 * @param pager           Open Pager instance (read/write).
 * @param first_page_num  Page number to initialize as the first leaf (MUST be >= 1).
 * @return TABLE_OK on success; a negative TableError (TABLE_E_*) on failure.
 */

int tblmgr_create(Pager* pager, uint32_t first_page_num);

/**
 * @brief Insert a new record into the table, automatically handling page chaining.
 *
 * This function searches the table pages in order for a free slot.
 * If all pages are full, a new page is allocated and linked.
 *
 * @param pager        Pointer to the Pager managing the file.
 * @param record_data  Pointer to the record data to be inserted.
 * @return 0-based global slot index (across all pages), or TABLE_E_* on error.
 */
int tblmgr_insert(Pager* pager, const void* record_data);

/**
 * @brief Iterate over all records in the table and invoke a callback for each used slot.
 *
 * The callback receives a pointer to the record data.
 * Returning non-zero from the callback stops the iteration early.
 *
 * @param pager     Pointer to the Pager managing the file.
 * @param callback  Function pointer called for each used record.
 * @return TABLE_OK on success, or the callback’s return value if it stops iteration.
 */
int tblmgr_scan(Pager* pager, int (*callback)(const void* record));

/**
 * @brief Delete (free) a record at the given global index.
 *
 * This optional helper computes the page and local slot index,
 * marks the slot as free, and updates the used count.
 *
 * @param pager  Pointer to the Pager managing the file.
 * @param id     Global record index (across pages).
 * @return TABLE_OK on success, TABLE_E_* on error.
 */
int tblmgr_delete(Pager* pager, uint32_t id);

/**
 * @brief Validate all pages of the table.
 *
 * This is a debugging utility that walks all linked pages and
 * runs tbl_validate() on each of them.
 *
 * @param pager Pointer to the Pager managing the file.
 * @param first_page_num First page number of the table.
 * @return TABLE_OK if all pages are valid, or the first TABLE_E_* encountered.
 */
int tblmgr_validate_all(Pager* pager, uint32_t first_page_num);

#endif // TABLE_MANAGER_H