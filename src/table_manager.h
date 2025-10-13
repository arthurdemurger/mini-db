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
 * @brief Insert a new 128-byte record into the table, allocating pages as needed.
 *
 * This function finds the first page with a free slot, or allocates a new
 * leaf page at the end of the file if all existing pages are full.
 * The new record is copied into the first free slot, and the used count
 * and bitmap are updated accordingly.
 *
 * @param p            Pointer to the Pager managing the file.
 * @param root_page_no Page number of the first leaf page of the table.
 * @param rec_128b     Pointer to the 128-byte record to insert.
 * @param out_id       Optional pointer to receive the global record index.
 *                     This is a zero-based index across all pages in the table.
 *                     If NULL, the index is not returned.
 * @return TABLE_OK on success, TABLE_E_* on error.
*/
int tblmgr_insert(Pager* p, uint32_t root_page_no, const void* rec_128b, uint32_t* out_id);


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