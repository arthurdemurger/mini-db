#include "table_manager.h"
#include <string.h>
#include "table.h"
#include <stdbool.h>
#include <stdlib.h>

int tblmgr_create(Pager* pager, uint32_t first_page_num) {
  if (!pager || first_page_num == 0)
    return TABLE_E_INVAL;

  while (first_page_num >= pager_page_count(pager)) {
    uint32_t new_no;
    int prc = pager_alloc_page(pager, &new_no);
    if (prc != PAGER_OK) {
      return TABLE_E_INVAL;
    }
  }

  size_t page_sz = pager_page_size(pager);
  uint8_t *buf = malloc(page_sz);
  if (!buf) return TABLE_E_INVAL;

  int rc = pager_read(pager, first_page_num, buf);
  if (rc < 0) { free(buf); return TABLE_E_INVAL; }

  bool all_zero = true;
  for (size_t i = 0; i < page_sz; i++) {
    if (buf[i] != 0) {
      all_zero = false;
      break;
    }
  }

  if (all_zero) {
    rc = tbl_init_leaf(buf, TABLE_RECORD_SIZE);
    if (rc != TABLE_OK) { free(buf); return rc; }

    rc = tbl_validate(buf);
    if (rc != TABLE_OK) { free(buf); return rc; }

    rc = pager_write(pager, first_page_num, buf);
    if (rc != PAGER_OK) { free(buf); return PAGER_E_INVAL; }

    free(buf);
    return TABLE_OK;
  }

  rc = tbl_validate(buf);
  if (rc != TABLE_OK) { free(buf); return rc; }

  if (tbl_get_record_size(buf) == TABLE_RECORD_SIZE &&
      tbl_get_used_count(buf)  == 0 &&
      tbl_get_next_page(buf)   == 0) {
    free(buf);
    return TABLE_OK;
  }

  free(buf);
  return TABLE_E_INVAL;
}
