# Mini-DB (C) — V1 (Pager + Table Leaf Design)

A tiny educational database engine in C that starts with a clean, low-level **pager** and a simple **table page (TABLE_LEAF)** layout.

---

## 🔧 Current State (V1)

✅ **Implemented:**
- Pager (open/read/close, fixed 4 KiB pages)
- In-page table operations (`tbl_init_leaf`, `tbl_validate`, `tbl_slot_find_free`)
- Bitmap management and validation (`bitmap_popcount`, extra-bit masking)
- Complete test suite for `pager.c` and `table.c`
- Updated Makefile with independent test targets (`test_pager.bin`, `test_table.bin`)

🧩 **In progress / upcoming:**
- `tbl_slot_mark_used` / `tbl_slot_mark_free` to update `used_count`
- Higher-level CRUD API (`table_insert`, `table_select_all`)
- Pager write/alloc extensions
- CLI (REPL) for manual testing

---

## 🚀 Quick Start

```bash
# Build everything and run both pager & table tests
make run

# Step-by-step
make fixtures   # build and run the fixtures generator
make tests      # build all test binaries
./tests/test_pager.bin
./tests/test_table.bin

# Clean everything
make clean
```

---

## 📁 Repository Layout

```
.
├── LICENSE
├── Makefile
├── README.md
├── docs/
├── include/
│   ├── pager.h           # public pager API
│   └── table.h           # in-page table operations & layout constants
├── src/
│   ├── endian_util.h     # little-endian helpers
│   ├── pager.c           # pager_open/read/close
│   ├── pager.h
│   ├── table.c           # TABLE_LEAF operations & validation
│   └── table.h
└── tests/
    ├── fixtures/
    │   ├── make_fixtures.c   # builds test fixture databases
    │   └── make_fixtures.bin
    ├── test_pager.c          # pager-level tests
    ├── test_table.c          # table leaf validation & bitmap tests
    ├── test_pager.bin
    └── test_table.bin
```

---

## 🧱 TABLE_LEAF — On-page Layout (V1)

### Structure (4096 B total)

```
+------------------------------+
| Header (24 B)                |
+------------------------------+
| Slot Bitmap (ceil(C/8) B)    |
+------------------------------+
| Data Area (C × 128 B)        |
+------------------------------+
```

Where **C = 31** for V1.

### Header (little-endian)

| Offset | Size | Field         | Description |
|:------:|:----:|---------------|--------------|
| 0 | 2 | kind | 0x0001 (TABLE_LEAF) |
| 2 | 2 | record_size | 128 |
| 4 | 2 | capacity | #slots (31) |
| 6 | 2 | used_count | occupied slots |
| 8 | 4 | next_page | next leaf (0=end) |
| 12 | 4 | reserved0 | future |
| 16 | 4 | reserved1 | future |
| 20 | 4 | reserved2 | future |

### Bitmap semantics
- **1 bit = slot used**, **0 bit = free**
- Stored **LSB-first** per byte
- Bits beyond `capacity` (in the last byte) **must be zero**

---

## 🧩 Validation Rules

A valid page must satisfy:

1. `kind == TABLE_PAGE_KIND_LEAF`
2. `record_size == 128`
3. `1 ≤ capacity` and `used_count ≤ capacity`
4. `popcount(bitmap) == used_count`
5. No bits beyond `capacity` set in the last byte
6. `data_offset(capacity) + cap * record_size ≤ TABLE_PAGE_SIZE`

Violations return `TABLE_E_*` codes.

---

## 🧪 Tests

`tests/test_table.c` exercises:

- ✅ `tbl_init_leaf()` initializes a clean header/bitmap
- ✅ `tbl_validate()` detects layout, bitmap, and popcount mismatches
- ✅ `tbl_slot_find_free()` returns correct first free slot and -1 when full
- ✅ Extra bits in the last bitmap byte trigger `TABLE_E_BITMAP`

Typical output:

```
$ make run
All pager tests passed.
table.c tests passed.
```

---

## 🛣️ Roadmap

- [x] Pager open/read/close
- [x] Fixtures & pager tests
- [x] TABLE_LEAF layout + capacity validation
- [x] tbl_init_leaf / tbl_validate / tbl_slot_find_free
- [x] Dedicated table test binary (`test_table.bin`)
- [ ] Slot mark/free + update used_count
- [ ] Table-level CRUD (`insert`, `select_all`)
- [ ] Pager write/alloc
- [ ] Simple REPL CLI
- [ ] (Later) Variable-length records / overflow pages

---

## 🪪 License

See [LICENSE](LICENSE).
