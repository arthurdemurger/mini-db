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
    int rc = pager_alloc_page(pager, &new_no);
    if (rc != PAGER_OK) {
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
    if (rc != PAGER_OK) { free(buf); return TABLE_E_INVAL; }

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

int tblmgr_insert(Pager* p, uint32_t root_page_no, const void* rec_128b, uint32_t* out_id)
{
  if (!p || root_page_no < 1 || !rec_128b) {
    return TABLE_E_INVAL;
  }

  const size_t pgsz = pager_page_size(p);
  uint8_t* buf = malloc(pgsz);
  if (!buf) return TABLE_E_INVAL;

  uint32_t page = root_page_no;

  // insert loop
  while (true) {
    int rc = pager_read(p, page, buf);
    if (rc != PAGER_OK) { free(buf); return TABLE_E_INVAL; }

    // Validate the table/leaf format
    rc = tbl_validate(buf);
    if (rc != TABLE_OK) { free(buf); return rc; }

    const uint16_t cap  = tbl_get_capacity(buf);
    const uint16_t used = tbl_get_used_count(buf);
    const uint32_t next = tbl_get_next_page(buf);

    // if there's space, insert here
    if (used < cap) {
      int idx = tbl_slot_find_free(buf);
      if (idx < 0) { free(buf); return TABLE_E_LAYOUT; }

      void *dst = tbl_slot_ptr(buf, (uint16_t) idx);
      if (!dst) { free(buf); return TABLE_E_INVAL; }

      memcpy(dst, rec_128b, TABLE_RECORD_SIZE);

      tbl_slot_mark_used(buf, (uint16_t)idx);

      rc = pager_write(p, page, buf);
      if (rc != PAGER_OK) { free(buf); return TABLE_E_INVAL; }

      // Compose a 32-bit logical record ID: (page << 16) | slot
      if (out_id)
        *out_id = (page << 16) | (uint32_t)idx;

      free(buf);
      return TABLE_OK;
    }

    // if the page is full, follow the linked chain if possible
    if (next != 0) {
      page = next;
      continue;
    }

    // End of chain and page is full → allocate and link a new page
    // (a) Allocate a new physical page
    uint32_t new_page;
    rc = pager_alloc_page(p, &new_page);
    if (rc != PAGER_OK) { free(buf); return TABLE_E_INVAL; }

    // (b) Prepare a fresh leaf page
    uint8_t* newbuf = calloc(1, pgsz);
    if (!newbuf) { free(buf); return TABLE_E_INVAL; }

    rc = tbl_init_leaf(newbuf, TABLE_RECORD_SIZE);
    if (rc != TABLE_OK) { free(newbuf); free(buf); return rc; }

    // Write the new page to disk before linking it
    rc = pager_write(p, new_page, newbuf);
    if (rc != PAGER_OK) { free(newbuf); free(buf); return TABLE_E_INVAL; }
    free(newbuf);

    // (c) Link the old full page to the new page
    tbl_set_next_page(buf, new_page);
    rc = pager_write(p, page, buf);
    if (rc != PAGER_OK) { free(buf); return TABLE_E_INVAL; }

    // Now continue on the new page in the next loop iteration
    page = new_page;
  }
}

int tblmgr_scan(Pager* pager,
                uint32_t root_page_no,
                int (*callback)(const void* record,
                                uint32_t record_id,
                                void* user_data),
                void* user_data)
{
  if (!pager || root_page_no == 0 || !callback)
    return TABLE_E_INVAL;

  const size_t pgsz = pager_page_size(pager);
  uint8_t* buf = (uint8_t*)malloc(pgsz);
  if (!buf) return TABLE_E_INVAL;

  uint32_t page = root_page_no;

  while (true) {
    // read current page
    int rc = pager_read(pager, page, buf);
    if (rc != PAGER_OK) { free(buf); return TABLE_E_INVAL; }

    // Validate table leaf page
    rc = tbl_validate(buf);
    if (rc != TABLE_OK) { free(buf); return rc; }

    const uint16_t cap  = tbl_get_capacity(buf);
    const uint32_t next = tbl_get_next_page(buf);

    const uint32_t page_count = pager_page_count(pager);
    if (next >= page_count && next != 0) { free(buf); return TABLE_E_LAYOUT; }
    // Visit all used slots and invoke the callback
    for (uint16_t i = 0; i < cap; i++) {
      if (!tbl_slot_is_used(buf, i))
        continue;

      const void* rec = tbl_slot_ptr(buf, i);
      uint32_t id = (page << 16) | (uint32_t)i;

      int cb_rc = callback(rec, id, user_data);
      if (cb_rc != 0) { free(buf); return cb_rc; }
    }

    if (next == 0)
      break;

    page = next;
  }

  free(buf);
  return TABLE_OK;
}

int tblmgr_delete(Pager* pager, uint32_t id) {
  if (!pager)
    return TABLE_E_INVAL;

  const uint32_t page_no  = id >> 16;
  const uint16_t slot_idx = (uint16_t)(id & 0xFFFF);

  // Page 0 is the file header; must be >= 1 and strictly less than page_count
  const uint32_t pcnt = pager_page_count(pager);
  if (page_no == 0 || page_no >= pcnt)
    return TABLE_E_INVAL;

  const size_t pgsz = pager_page_size(pager);
  uint8_t* buf = (uint8_t*)malloc(pgsz);
  if (!buf) return TABLE_E_INVAL;

  // Read page and validate table leaf layout
  int rc = pager_read(pager, page_no, buf);
  if (rc != PAGER_OK) { free(buf); return TABLE_E_INVAL; }

  rc = tbl_validate(buf);
  if (rc != TABLE_OK) { free(buf); return rc; }

  // Basic invariants and slot bounds
  const uint16_t cap = tbl_get_capacity(buf);
  if (slot_idx >= cap) { free(buf); return TABLE_E_INVAL; }

  // Must be currently used to delete it
  if (!tbl_slot_is_used(buf, slot_idx)) {
    free(buf);
    return TABLE_E_INVAL;
  }

  // Reset the slot
  void* rec_ptr = tbl_slot_ptr(buf, slot_idx);
  if (rec_ptr) {
    memset(rec_ptr, 0, TABLE_RECORD_SIZE);
  }

  // Mark slot free
  tbl_slot_mark_free(buf, slot_idx);

  // Persist updated page
  rc = pager_write(pager, page_no, buf);
  free(buf);
  if (rc != PAGER_OK) return TABLE_E_INVAL;

  return TABLE_OK;
}

int tblmgr_validate_all(Pager* pager, uint32_t first_page_num) {
  if (!pager || first_page_num == 0)
    return TABLE_E_INVAL;

  const size_t pgsz = pager_page_size(pager);
  uint8_t* buf = (uint8_t*)malloc(pgsz);
  if (!buf) return TABLE_E_INVAL;

  const uint32_t page_count = pager_page_count(pager);
  uint32_t page = first_page_num;

  while (true) {
    // Range check before read (avoid reading header or out-of-range)
    if (page == 0 || page >= page_count) { free(buf); return TABLE_E_LAYOUT; }

    // Read page
    int prc = pager_read(pager, page, buf);
    if (prc != PAGER_OK) { free(buf); return TABLE_E_INVAL; }

    // Validate table/leaf page invariants
    int trc = tbl_validate(buf);
    if (trc != TABLE_OK) { free(buf); return trc; }

    // Get next page and perform basic sanity checks
    const uint32_t next = tbl_get_next_page(buf);

    if (next == 0) {
      // End of chain
      break;
    }

    // next must be within file page range
    if (next >= page_count) { free(buf); return TABLE_E_LAYOUT; }

    page = next;
  }

  free(buf);
  return TABLE_OK;
}

int tblmgr_get(Pager* pager, uint32_t id, void* out_rec128) {
  if (!pager || !out_rec128) return TABLE_E_INVAL;

  const uint32_t page_no  = id >> 16;
  const uint16_t slot_idx = (uint16_t)(id & 0xFFFF);

  const uint32_t page_count = pager_page_count(pager);
  if (page_no == 0 || page_no >= page_count) return TABLE_E_INVAL;

  const size_t page_sz = pager_page_size(pager);
  uint8_t* buf = (uint8_t*)malloc(page_sz);
  if (!buf) return TABLE_E_INVAL;

  int rc = pager_read(pager, page_no, buf);
  if (rc != PAGER_OK) { free(buf); return TABLE_E_INVAL; }

  rc = tbl_validate(buf);
  if (rc != TABLE_OK) { free(buf); return rc; }

  const uint16_t cap = tbl_get_capacity(buf);
  if (slot_idx >= cap) { free(buf); return TABLE_E_INVAL; }
  if (!tbl_slot_is_used(buf, slot_idx)) { free(buf); return TABLE_E_INVAL; }

  const void* src = tbl_slot_ptr(buf, slot_idx);
  if (!src) { free(buf); return TABLE_E_INVAL; }

  memcpy(out_rec128, src, TABLE_RECORD_SIZE);
  free(buf);
  return TABLE_OK;
}

int tblmgr_update(Pager* pager, uint32_t id, const void* rec_128b) {
  if (!pager || !rec_128b) return TABLE_E_INVAL;

  const uint32_t page_no  = id >> 16;
  const uint16_t slot_idx = (uint16_t)(id & 0xFFFF);

  const uint32_t page_count = pager_page_count(pager);
  if (page_no == 0 || page_no >= page_count) return TABLE_E_INVAL;

  const size_t page_sz = pager_page_size(pager);
  uint8_t* buf = (uint8_t*)malloc(page_sz);
  if (!buf) return TABLE_E_INVAL;

  int rc = pager_read(pager, page_no, buf);
  if (rc != PAGER_OK) { free(buf); return TABLE_E_INVAL; }

  rc = tbl_validate(buf);
  if (rc != TABLE_OK) { free(buf); return rc; }

  const uint16_t cap = tbl_get_capacity(buf);
  if (slot_idx >= cap) { free(buf); return TABLE_E_INVAL; }
  if (!tbl_slot_is_used(buf, slot_idx)) { free(buf); return TABLE_E_INVAL; }

  const void* dst = tbl_slot_ptr(buf, slot_idx);
  if (!dst) { free(buf); return TABLE_E_INVAL; }

  memcpy(dst, rec_128b, TABLE_RECORD_SIZE);

  rc = pager_write(pager, page_no, buf);
  free(buf);
  if (rc != PAGER_OK) return TABLE_E_INVAL;

  return TABLE_OK;
}
