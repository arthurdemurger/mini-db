# Mini-DB (C) — V1 (Pager + Table Leaf Design)

A tiny educational database engine in C that starts with a clean, low-level **pager** and a simple **table page (TABLE_LEAF)** layout.

- Paginated on-disk file (fixed page size: **4 KiB**)
- Robust I/O with `pread` (handles EINTR + short reads)
- Opaque API and modular design
- **NEW (V1 decisions):** fixed-size records (**128 B**) stored contiguously in **TABLE_LEAF** pages with a compact **slot bitmap**

---

## Quick start

```sh
# Generate fixtures and run pager tests
make run

# Or step-by-step
make fixtures
make tests
./tests/test_pager.bin

# Clean everything
make clean
```

---

## Repository layout

```
.
├── include/
│   ├── pager.h           # public pager API
│   └── table.h           # (soon) in-page table operations & high-level table API
├── src/
│   ├── pager.c           # pager_open/read/close + helpers
│   └── table.c           # (soon) TABLE_LEAF ops + basic CRUD scaffolding
├── tests/
│   ├── fixtures/
│   │   └── make_fixtures.c  # generates *.db test files
│   ├── test_pager.c         # black-box tests for pager
│   └── test_table.c         # (soon) unit tests for table pages & CRUD
└── docs/
    └── TABLE_LEAF.md        # (soon) detailed on-disk layout and invariants
```

---

## Pager — Public API (unchanged)

```c
// include/pager.h
typedef struct Pager Pager;

int      pager_open(const char* path, Pager** out);
int      pager_read(const Pager* p, uint32_t page_no, void* out_page_buf);
void     pager_close(Pager* p);
size_t   pager_page_size(const Pager* p);
uint32_t pager_page_count(const Pager* p);
const char* pager_errstr(int code);
```

- Returns `0` on success, negative codes on error.
- The `Pager` type is opaque (implementation details hidden).

### On-disk file header (page 0, v1)

| Offset | Size | Field      | Value (v1)        |
|------: |----: |------------|-------------------|
| 0      | 4    | magic      | `"MDB1"`          |
| 4      | 4    | version    | `1`               |
| 8      | 4    | page_size  | `4096`            |
| 12     | 4    | page_count | `>= 1`            |
| 16     | 4    | flags      | `0`               |

All multi-byte integers are **little-endian** on disk.

---

## NEW — TABLE_LEAF (V1 layout & decisions)

A **TABLE_LEAF** page stores user records of **fixed size** (V1 = 128 bytes), plus tiny metadata.

### Page format (4096 B total)
```
+------------------------------+
| Header (24 B)                |
+------------------------------+
| Slot Bitmap (ceil(C/8) B)    |
+------------------------------+
| Data Area (C * 128 B)        |
+------------------------------+
```
Where **C** is the number of slots (records) in the page.

### Header (24 B, little-endian)
| Offset | Size | Field         | Description                                      |
|------: |----: |---------------|--------------------------------------------------|
| 0      | 2    | kind          | `0x0001` (TABLE_LEAF)                            |
| 2      | 2    | record_size   | `128` (V1)                                       |
| 4      | 2    | capacity      | slots per page (V1: **31**)                      |
| 6      | 2    | used_count    | # of occupied slots                              |
| 8      | 4    | next_page     | next TABLE_LEAF page (0 = end)                   |
| 12     | 4    | reserved0     | 0 (future use)                                   |
| 16     | 4    | reserved1     | 0 (future use)                                   |
| 20     | 4    | reserved2     | 0 (future use)                                   |

- **Bitmap**: `ceil(capacity/8)` bytes (V1: `ceil(31/8) = 4` bytes). Bit=1 → slot used; Bit=0 → free.
- **Data**: records stored **contiguously**. Address of `record[i]` = `data_base + i * 128`.

### Capacity computation (V1 values)
We ensure: `header(24) + bitmap(ceil(C/8)) + C * 128 ≤ 4096` → **C = 31**.

### Record layout (example, 128 B)
A concrete example for prototyping CRUD (you can adjust later):
```
u32   id;
u16   flags;
u16   name_len;
char  name[32];   // UTF-8, padded/truncated
u16   email_len;
char  email[64];  // UTF-8, padded/truncated
u8    age;
u8    nullmask;   // bit0: name null, bit1: email null, ...
u8    _pad[21];   // to reach 128 bytes
```

### Invariants (must hold for a valid page)
- `kind == 0x0001`
- `record_size == 128`
- `1 ≤ capacity` and `used_count ≤ capacity`
- `popcount(bitmap) == used_count`
- No bits beyond `capacity` set in the last bitmap byte
- `next_page == 0` or a valid page number (checked at higher level)

---

## Table module — scope & upcoming APIs

**In-page operations** (`table.h/.c`):
```c
int   tbl_init_leaf(void* page, uint16_t record_size /*=128*/);
int   tbl_validate(const void* page);                 // check invariants
int   tbl_slot_find_free(const void* page);           // returns index or -1
int   tbl_slot_mark_used(void* page, int idx);
int   tbl_slot_mark_free(void* page, int idx);
void* tbl_slot_ptr(void* page, int idx);              // pointer to record[i]
```

**High-level table (single-table v1)**:
```c
typedef struct Table {
  Pager*    p;
  uint32_t  root;           // root page number (first TABLE_LEAF)
  uint16_t  record_size;    // 128 for V1
} Table;

int table_open(Pager* p, uint32_t root_page, uint16_t record_size, Table* out);
int table_insert(Table* t, const void* rec128);       // append to first free slot; alloc if full
int table_select_all(Table* t, int (*cb)(const void* rec, void* u), void* u);
```

**Pager tiny extensions (planned)**:
```c
int pager_write(Pager* p, uint32_t page_no, const void* buf);
int pager_alloc_page(Pager* p, uint32_t* out_page_no);   // append-only growth
```

---

## Testing (current & planned)

- **Pager tests** (existing): fixtures & black-box tests on open/read/getters.
- **Table tests** (planned):
  - `tbl_init_leaf` initializes header + bitmap correctly
  - insert/select within one page → verify bitmap & `used_count`
  - grow to multiple pages → verify `next_page` chain
  - validation rejects malformed pages (wrong kind, bad bitmap, etc.)

Typical run (current):
```sh
make run
# Fixtures created in tests/fixtures/
# All pager tests passed.
```

---

## Roadmap

- [x] Pager open/read/close
- [x] Fixtures + tests
- [x] V1 TABLE_LEAF design (128 B records, bitmap, capacity=31)
- [ ] Implement `table.h/.c` in-page ops
- [ ] Implement `pager_write` + `pager_alloc_page`
- [ ] Minimal INSERT/SELECT over chained TABLE_LEAF pages
- [ ] CLI (REPL) for quick manual tests
- [ ] (Later) Overflow pages / variable-length records
- [ ] (Later) Secondary index / journaling

---

## License

See [LICENSE](LICENSE).
