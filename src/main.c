#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "cli_format.h"
#include "pager.h"
#include "table_manager.h"
#include "table.h"

static void die(const char* msg) { fprintf(stderr, "%s\n", msg); exit(2); }

static int read_file(const char* path, uint8_t* buf, size_t cap) {
  FILE* f = fopen(path, "rb");
  if (!f) { perror("fopen"); return -1; }
  size_t n = fread(buf, 1, cap, f);
  if (n != cap) { fprintf(stderr, "expected %zu bytes, got %zu\n", cap, n); fclose(f); return -1; }
  fclose(f);
  return 0;
}

static void cmd_create(Pager* p, uint32_t root) {
  int rc = tblmgr_create(p, root);
  if (rc != TABLE_OK) { fprintf(stderr, "create failed rc=%d\n", rc); exit(1); }
  printf("created table at page %u\n", root);
}

static void cmd_insert(Pager* p, uint32_t root, const char* file128) {
  uint8_t rec[128];
  if (read_file(file128, rec, sizeof rec) != 0) exit(1);
  uint32_t id = 0;
  int rc = tblmgr_insert(p, root, rec, &id);
  if (rc != TABLE_OK) { fprintf(stderr, "insert failed rc=%d\n", rc); exit(1); }
  printf("%u\n", id);
}

static void cmd_get(Pager* p, uint32_t id) {
  uint8_t rec[128];
  int rc = tblmgr_get(p, id, rec);
  if (rc != TABLE_OK) { fprintf(stderr, "get failed rc=%d\n", rc); exit(1); }
  // dump as hex
  for (size_t i = 0; i < sizeof rec; i++) {
    printf("%02x", rec[i]);
    if ((i+1)%16==0) printf("\n"); else printf(" ");
  }
}

static void cmd_update(Pager* p, uint32_t id, const char* file128) {
  uint8_t rec[128];
  if (read_file(file128, rec, sizeof rec) != 0) exit(1);
  int rc = tblmgr_update(p, id, rec);
  if (rc != TABLE_OK) { fprintf(stderr, "update failed rc=%d\n", rc); exit(1); }
  printf("ok\n");
}

static void cmd_delete(Pager* p, uint32_t id) {
  int rc = tblmgr_delete(p, id);
  if (rc != TABLE_OK) { fprintf(stderr, "delete failed rc=%d\n", rc); exit(1); }
  printf("ok\n");
}

static int scan_cb(const void* rec, uint32_t id, void* ud) {
  (void)rec;
  FILE* out = (FILE*)ud;
  fprintf(out, "%u\n", id);
  return 0;
}


static void cmd_scan(Pager* p, uint32_t root) {
  int rc = tblmgr_scan(p, root, scan_cb, stdout);
  if (rc != TABLE_OK) { fprintf(stderr, "scan failed rc=%d\n", rc); exit(1); }
}

static void cmd_validate(Pager* p, uint32_t root) {
  int rc = tblmgr_validate_all(p, root);
  if (rc != TABLE_OK) { fprintf(stderr, "validate failed rc=%d\n", rc); exit(1); }
  printf("ok\n");
}

// getf <id> <spec>
static void cmd_getf(Pager* p, uint32_t id, const char* spec_str) {
  unsigned char rec[128];
  if (tblmgr_get(p, id, rec) != TABLE_OK) { fprintf(stderr, "get %u failed\n", id); exit(1); }

  char buf[512]; strncpy(buf, spec_str, sizeof buf - 1); buf[sizeof buf - 1] = 0;
  FieldSpec fs;
  if (parse_spec(buf, &fs) != 0) { fprintf(stderr, "bad spec\n"); exit(2); }

  print_header_spec(&fs);
  print_row_spec(id, &fs, rec);
  print_footer_spec(&fs, 1);
}

// listf <root> <spec>
static void cmd_listf(Pager* p, uint32_t root, const char* spec_str) {
  char buf[512]; strncpy(buf, spec_str, sizeof buf - 1); buf[sizeof buf - 1] = 0;
  FieldSpec fs;
  if (parse_spec(buf, &fs) != 0) { fprintf(stderr, "bad spec\n"); exit(2); }

  print_header_spec(&fs);
  size_t n = 0;
  ListfCtx ctx = { .fs = &fs, .counter = &n };
  int rc = tblmgr_scan(p, root, scan_cb_listf, &ctx);
  if (rc != TABLE_OK) { fprintf(stderr, "scan failed rc=%d\n", rc); exit(1); }
  print_footer_spec(&fs, n);
}


static void usage(const char* prog) {
  fprintf(stderr,
    "Usage:\n"
    "  %s <db> create <root_page>\n"
    "  %s <db> insert <root_page> <file_128bytes>\n"
    "  %s <db> get <id>\n"
    "  %s <db> update <id> <file_128bytes>\n"
    "  %s <db> delete <id>\n"
    "  %s <db> scan <root_page>\n"
    "  %s <db> validate <root_page>\n"
    "  %s <db> listf <root_page> <spec>\n"
    "  %s <db> getf  <id>        <spec>\n",
    prog, prog, prog, prog, prog, prog, prog, prog, prog);
  exit(2);
}

static void print_hex(const void* data, size_t n) {
  const unsigned char* p = (const unsigned char*)data;
  for (size_t i = 0; i < n; i++) {
    if ((i % 16) == 0) printf("%08zx  ", i);
    printf("%02x%s", p[i], ((i+1)%8==0) ? "  " : " ");
    if ((i % 16) == 15) printf("\n");
  }
  if ((n % 16) != 0) printf("\n");
}

static void cmd_inspect(Pager* p, uint32_t root) {
  // buffer pour une page
  unsigned char pagebuf[4096];
  uint32_t page_no = root;
  uint64_t hop = 0, total_used = 0;
  const uint64_t HOP_LIMIT = 1000000; // garde-fou contre boucles

  printf("DB inspect (root=%u)\n", root);
  printf("Chain: ");

  while (page_no != 0) {
    if (++hop > HOP_LIMIT) { fprintf(stderr, "chain too long / loop?\n"); break; }

    if (pager_read(p, page_no, pagebuf) != PAGER_OK) {
      fprintf(stderr, "read page %u failed\n", page_no);
      break;
    }
    // valide & récupère les champs
    if (tbl_validate(pagebuf) != TABLE_OK) {
      fprintf(stderr, "page %u invalid\n", page_no);
      break;
    }

    uint16_t kind        = tbl_get_kind(pagebuf);
    uint16_t rec_size    = tbl_get_record_size(pagebuf);
    uint16_t capacity    = tbl_get_capacity(pagebuf);
    uint16_t used        = tbl_get_used_count(pagebuf);
    uint32_t next        = tbl_get_next_page(pagebuf);

    if (page_no == root) printf("%u", page_no); else printf(" -> %u", page_no);

    printf("\n  page %u: kind=%u rec_size=%u capacity=%u used=%u next=%u\n",
           page_no, kind, rec_size, capacity, used, next);

    total_used += used;
    if (next == 0) break;
    page_no = next;
  }
  printf("\nTotal rows (sum used): %llu\n", (unsigned long long)total_used);
}

static void cmd_dump_page(Pager* p, uint32_t page_no) {
  unsigned char pagebuf[4096];
  if (pager_read(p, page_no, pagebuf) != PAGER_OK) {
    fprintf(stderr, "read page %u failed\n", page_no);
    exit(1);
  }
  printf("Page %u (4096 bytes):\n", page_no);
  print_hex(pagebuf, sizeof pagebuf);
}

static void cmd_dump_row(Pager* p, uint32_t id) {
  unsigned char rec[128];
  if (tblmgr_get(p, id, rec) != TABLE_OK) {
    fprintf(stderr, "get %u failed\n", id);
    exit(1);
  }
  printf("Row %u (128 bytes):\n", id);
  print_hex(rec, sizeof rec);
}

int main(int argc, char** argv) {
  if (argc < 3) usage(argv[0]);
  const char* db = argv[1];
  const char* cmd = argv[2];

  Pager* p = NULL;
  if (pager_open(db, &p) != PAGER_OK) die("pager_open failed");

  if (strcmp(cmd, "create")==0) {
    if (argc != 4) usage(argv[0]);
    uint32_t root = (uint32_t)strtoul(argv[3], NULL, 10);
    cmd_create(p, root);
  } else if (strcmp(cmd, "insert")==0) {
    if (argc != 5) usage(argv[0]);
    uint32_t root = (uint32_t)strtoul(argv[3], NULL, 10);
    cmd_insert(p, root, argv[4]);
  } else if (strcmp(cmd, "get")==0) {
    if (argc != 4) usage(argv[0]);
    uint32_t id = (uint32_t)strtoul(argv[3], NULL, 10);
    cmd_get(p, id);
  } else if (strcmp(cmd, "update")==0) {
    if (argc != 5) usage(argv[0]);
    uint32_t id = (uint32_t)strtoul(argv[3], NULL, 10);
    cmd_update(p, id, argv[4]);
  } else if (strcmp(cmd, "delete")==0) {
    if (argc != 4) usage(argv[0]);
    uint32_t id = (uint32_t)strtoul(argv[3], NULL, 10);
    cmd_delete(p, id);
  } else if (strcmp(cmd, "scan")==0) {
    if (argc != 4) usage(argv[0]);
    uint32_t root = (uint32_t)strtoul(argv[3], NULL, 10);
    cmd_scan(p, root);
  } else if (strcmp(cmd, "validate")==0) {
    if (argc != 4) usage(argv[0]);
    uint32_t root = (uint32_t)strtoul(argv[3], NULL, 10);
    cmd_validate(p, root);
  } else if (strcmp(cmd, "inspect")==0) {
    if (argc != 4) usage(argv[0]);
    uint32_t root = (uint32_t)strtoul(argv[3], NULL, 10);
    cmd_inspect(p, root);
  } else if (strcmp(cmd, "dump")==0) {
    if (argc < 5) usage(argv[0]);
    const char* what = argv[3];
    if (strcmp(what, "page")==0) {
      uint32_t pg = (uint32_t)strtoul(argv[4], NULL, 10);
      cmd_dump_page(p, pg);
    } else if (strcmp(what, "row")==0) {
      uint32_t id = (uint32_t)strtoul(argv[4], NULL, 10);
      cmd_dump_row(p, id);
    }
  } else if (strcmp(cmd, "listf")==0) {
    if (argc != 5) usage(argv[0]);
    uint32_t root = (uint32_t)strtoul(argv[3], NULL, 10);
    const char* spec = argv[4];
    cmd_listf(p, root, spec);
  } else if (strcmp(cmd, "getf")==0) {
    if (argc != 5) usage(argv[0]);
    uint32_t id = (uint32_t)strtoul(argv[3], NULL, 10);
    const char* spec = argv[4];
    cmd_getf(p, id, spec);
  } else {
    usage(argv[0]);
  }

  pager_close(p);
  return 0;
}
