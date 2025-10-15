// tests/test_table_manager.c
// Black-box tests for table_manager: create/insert/get/update/delete/scan/validate_all.
//
// Strategy:
//  - Copy fixtures/valid.db to a temp file (so we don't depend on pager_open's "new file" path).
//  - Open with pager_open, pick a fresh root_page >= current page_count, run tblmgr_create.
//  - Insert > capacity to force page chaining.
//  - Verify with get / scan, then delete and update, then validate_all.
//
// Build: compile alongside your other tests.

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "pager.h"
#include "table_manager.h"
#include "table.h"

// ---- small file copy helper (for tmp db from fixtures) ----------------------
static int copy_file(const char* src, const char* dst) {
  FILE* in = fopen(src, "rb");
  if (!in) return -1;
  FILE* out = fopen(dst, "wb");
  if (!out) { fclose(in); return -1; }
  char buf[1 << 15];
  size_t n;
  while ((n = fread(buf, 1, sizeof buf, in)) > 0) {
    if (fwrite(buf, 1, n, out) != n) { fclose(in); fclose(out); return -1; }
  }
  fclose(in);
  fclose(out);
  return 0;
}

// ---- helpers to craft 128-byte payloads ------------------------------------
static void make_record(uint8_t rec[128], uint32_t tag) {
  // Simple deterministic pattern: first 4 bytes = tag (LE), rest = repeated tag bytes
  memset(rec, 0, 128);
  rec[0] = (uint8_t)(tag & 0xFF);
  rec[1] = (uint8_t)((tag >> 8) & 0xFF);
  rec[2] = (uint8_t)((tag >> 16) & 0xFF);
  rec[3] = (uint8_t)((tag >> 24) & 0xFF);
  for (int i = 4; i < 128; i++) rec[i] = (uint8_t)(tag + i);
}

static void make_record_alt(uint8_t rec[128], uint32_t tag) {
  // Alternate pattern to validate update()
  for (int i = 0; i < 128; i++) rec[i] = (uint8_t)(0xA5 ^ ((tag + i) & 0xFF));
}

// ---- scan callback used by tests -------------------------------------------
typedef struct {
  size_t seen;
  uint32_t forbid_id;   // 0 if none
  int forbid_seen;      // set to 1 if forbid_id is encountered
} ScanCtx;

static int count_and_check_cb(const void* rec, uint32_t id, void* user_data) {
  (void)rec;
  ScanCtx* ctx = (ScanCtx*)user_data;
  ctx->seen++;
  if (ctx->forbid_id != 0 && id == ctx->forbid_id) ctx->forbid_seen = 1;
  return 0; // continue
}

// ---- single end-to-end test -------------------------------------------------
static void test_table_manager_e2e(void) {
  // 1) Prepare temp DB from fixture
  const char* src = "tests/fixtures/valid.db";
  const char* tmp = "tests/tmp_tblmgr.db";
  assert(copy_file(src, tmp) == 0 && "copy fixture → tmp failed");

  // 2) Open pager on temp DB
  Pager* p = NULL;
  int rc = pager_open(tmp, &p);
  assert(rc == PAGER_OK && p);

  // 3) Choose a fresh root page number: start at current page_count (will force alloc)
  uint32_t root = pager_page_count(p);
  // Create table at 'root' (the function must allocate pages up to 'root' and init leaf)
  rc = tblmgr_create(p, root);
  assert(rc == TABLE_OK);

  // 4) Read the freshly created root page to obtain capacity
  size_t ps = pager_page_size(p);
  uint8_t* buf = (uint8_t*)malloc(ps);
  assert(buf);
  rc = pager_read(p, root, buf);
  assert(rc == PAGER_OK);
  assert(tbl_validate(buf) == TABLE_OK);

  uint16_t cap = tbl_get_capacity(buf);
  assert(cap >= 1);
  free(buf);

  // 5) Insert cap + 3 records to force chain growth
  //    Keep their IDs and payloads for later verification.
  const size_t N = (size_t)cap + 3;
  uint32_t* ids = (uint32_t*)malloc(N * sizeof(uint32_t));
  assert(ids);

  for (size_t i = 0; i < N; i++) {
    uint8_t rec[128];
    make_record(rec, (uint32_t)i);
    uint32_t id = 0;
    rc = tblmgr_insert(p, root, rec, &id);
    assert(rc == TABLE_OK);
    assert(id != 0);
    ids[i] = id;
  }

  // 6) GET a few records (first, middle, last) and verify their content
  for (size_t k = 0; k < 3; k++) {
    size_t idx = (k == 0) ? 0 : (k == 1 ? N / 2 : N - 1);
    uint8_t out[128], expect[128];
    memset(out, 0xEE, 128);
    make_record(expect, (uint32_t)idx);
    rc = tblmgr_get(p, ids[idx], out);
    assert(rc == TABLE_OK);
    assert(memcmp(out, expect, 128) == 0 && "GET must return the original payload");
  }

  // 7) UPDATE the middle record and verify
  {
    size_t mid = N / 2;
    uint8_t upd[128], out[128];
    make_record_alt(upd, (uint32_t)mid);
    rc = tblmgr_update(p, ids[mid], upd);
    assert(rc == TABLE_OK);
    memset(out, 0, 128);
    rc = tblmgr_get(p, ids[mid], out);
    assert(rc == TABLE_OK);
    assert(memcmp(out, upd, 128) == 0 && "UPDATE must persist new payload");
  }

  // 8) DELETE the last record, then scan and ensure it's not visited
  {
    size_t last = N - 1;
    rc = tblmgr_delete(p, ids[last]);
    assert(rc == TABLE_OK);

    ScanCtx ctx = {0};
    ctx.forbid_id = ids[last];
    rc = tblmgr_scan(p, root, count_and_check_cb, &ctx);
    assert(rc == TABLE_OK);
    // We inserted N records, deleted 1 → should see N-1 alive
    assert(ctx.seen == (N - 1));
    assert(ctx.forbid_seen == 0 && "deleted record must not be visited by scan");
  }

  // 9) validate_all: the entire chain must be consistent
  rc = tblmgr_validate_all(p, root);
  assert(rc == TABLE_OK);

  // 10) cleanup
  pager_close(p);
  // remove tmp file
  remove(tmp);
}

int main(void) {
  test_table_manager_e2e();
  printf("All table_manager tests passed.\n");
  return 0;
}
