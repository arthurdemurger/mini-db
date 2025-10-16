# Mini-DB (C) — Educational Storage Engine

A compact and pedagogical database engine written in **C**, designed to explore the internals of paged storage, record management, and simple query layers.

---

## 🌟 Overview

Mini-DB implements a minimal but complete data storage stack:

| Layer | Purpose | Key Files |
|-------|----------|-----------|
| **Pager** | Handles low-level file I/O, page caching and allocation (4 KiB pages). | `src/pager.c`, `src/pager.h` |
| **Table** | Manages records inside a fixed-size leaf page. | `src/table.c`, `src/table.h` |
| **Table Manager** | CRUD operations, page chaining, record IDs. | `src/table_manager.c`, `src/table_manager.h` |
| **CLI** | Command-line interface to manipulate the database. | `src/main.c` |
| **Formatter** | Generic pretty-printer for tabular records. | `src/cli_format.c`, `src/cli_format.h` |

---

## ✅ Current State (as of this version)

**Fully implemented and tested:**

- Pager: open/read/write/alloc/close with integrity checks.
- Table: leaf page validation, bitmap management, slot operations.
- Table Manager: complete CRUD + scan + validation across chained pages.
- CLI: `create`, `insert`, `get`, `update`, `delete`, `scan`, `validate`, `inspect`, `dump`, and tabular output (`listf`, `getf`).
- Formatter module for generic pretty-print of records.
- Robust Makefile with sanitizer options and colorized output.
- Functional test suites for each layer (`test_pager`, `test_table`, `test_table_manager`).
- Scenario scripts (`scripts/hex_scenario.sh`, `scripts/classic_scenario.sh`) for end-to-end demos.

---

## ⚙️ Build & Run

### Prerequisites
- GCC or Clang with C11 support.
- GNU Make.

### Build everything
```bash
make all
```

### Run all tests
```bash
make test
```

### Run demo scenario (end-to-end)
```bash
make hex_scenario
```

### Run classic example (tabular output)
```bash
make classic_scenario
```

The classic scenario creates a database with a table of 128‑byte records (name, age, city, note) and shows all CLI commands.

---

## 🧱 Table Page Layout (4 KiB)

```
+------------------------------+
| Header (24 B)                |
+------------------------------+
| Slot Bitmap (ceil(C/8) B)    |
+------------------------------+
| Record Area (C × 128 B)      |
+------------------------------+
```

### Header (little-endian)

| Offset | Size | Field | Description |
|:------:|:----:|:------|:-------------|
| 0 | 2 | kind | `0x0001` (TABLE_LEAF) |
| 2 | 2 | record_size | Usually 128 |
| 4 | 2 | capacity | Record slots (≈31) |
| 6 | 2 | used_count | Number of used records |
| 8 | 4 | next_page | Chained page (0=end) |
| 12–23 | 12 | reserved | future use |

Bitmap bits beyond capacity must be 0. Validation ensures `popcount(bitmap) == used_count`.

---

## 🧩 Record ID Encoding

Each record ID is a 32‑bit value:  
`id = (page_no << 16) | slot_index`

This allows up to 65 535 records per page and billions of pages overall.

---

## 💻 CLI Commands

### Basic CRUD
| Command | Usage | Description |
|----------|-------|-------------|
| `create` | `<db> create <root_page>` | Initialize a table at given page. |
| `insert` | `<db> insert <root_page> <record_file>` | Insert a 128‑byte record. |
| `get` | `<db> get <id>` | Dump record bytes in hex. |
| `update` | `<db> update <id> <record_file>` | Replace record content. |
| `delete` | `<db> delete <id>` | Mark slot free. |
| `scan` | `<db> scan <root_page>` | List all IDs (one per line). |
| `validate` | `<db> validate <root_page>` | Validate chain of pages. |

### Inspection & Debug
| Command | Usage | Description |
|----------|-------|-------------|
| `inspect` | `<db> inspect <root_page>` | Show structure of all linked pages. |
| `dump` | `<db> dump page <no>` / `dump row <id>` | Hex‑dump a page or record. |

### Tabular Display (Generic Formatter)
| Command | Usage | Description |
|----------|-------|-------------|
| `listf` | `<db> listf <root_page> <spec>` | Display all rows as a table based on a field spec. |
| `getf` | `<db> getf <id> <spec>` | Display a single record in tabular form. |

### Spec Format
```
name:offset:length:type[,name:offset:length:type...]
```
- `s` = NUL‑padded string  
- `u8`, `u16`, `u32` = integers (little‑endian)  
- `hex` = bytes as hex pairs

Example for 128‑byte classic layout:
```
"name:0:32:s,age:32:1:u8,city:33:32:s,note:65:63:s"
```

---

## 🧪 Testing

| Test File | Purpose |
|------------|----------|
| `tests/test_pager.c` | Validates file header, page read/write, I/O robustness. |
| `tests/test_table.c` | Checks page structure, bitmap logic, and validation rules. |
| `tests/test_table_manager.c` | End‑to‑end CRUD and multi‑page chaining tests. |

To run all:
```bash
make test
```

All tests must print *“All tests passed”*.

---

## 📜 Example Output

```
$ ./scripts/classic_scenario.sh
[5/10] pretty list (table view)
+--------+----------------+-----+--------------+--------------------------------+
| ID     | name           | age | city         | note                           |
+--------+----------------+-----+--------------+--------------------------------+
|  65536 | Alice          |  30 | Paris        | Loves baguettes                |
|  65537 | Bob            |  25 | Lyon         | Enjoys silk history            |
|  65538 | Carol          |  28 | Tokyo        | Karaoke on Fridays             |
+--------+----------------+-----+--------------+--------------------------------+
3 row(s)
```

---

## 🧰 Makefile Highlights

- Colorized compilation and test output with progress counters.
- `ASAN` / `UBSAN` support:
  ```bash
  make clean && make ASAN=1 UBSAN=1 test
  ```
- `make scenario` runs a scripted CRUD demo.
- `make check` builds and runs everything under sanitizers.

---

## 🧭 Project Structure

```
src/
 ├── pager.c/.h
 ├── table.c/.h
 ├── table_manager.c/.h
 ├── cli_format.c/.h      # generic field parser + table printer
 ├── endian_util.h
 └── main.c               # CLI (uses pager + table_manager + formatter)
tests/
 ├── test_pager.c
 ├── test_table.c
 └── test_table_manager.c
scripts/
 ├── run_scenario.sh
 └── classic_scenario.sh
Makefile
README.md
```

---

## 🧭 Roadmap

- [x] Pager read/write/alloc
- [x] Page validation & slot ops
- [x] Table Manager CRUD
- [x] CLI with debug + tabular output
- [x] Tests & demo scripts
- [ ] Index pages
- [ ] Variable‑length records
- [ ] Mini SQL‑like layer

---

## 🪪 License

MIT License — © 2025 Educational Mini‑DB Project.
