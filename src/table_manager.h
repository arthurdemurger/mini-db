#ifndef TABLE_MANAGER_H
#define TABLE_MANAGER_H

#include <stdint.h>
#include "pager.h"
#include "table.h"

/**
 * @brief Initialize a new table in the database file.
 *
 * This function allocates the first leaf page (page 0 by default)
 * and initializes it with the correct layout.
 *
 * @param pager  Pointer to an open Pager instance.
 * @param first_page_num  Page number to initialize as the first page (usually 0).
 * @return TABLE_OK on success, TABLE_E_* on failure.
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