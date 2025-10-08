# Mini-DB (C) — Pager v1

A tiny educational database engine in C that focuses on a clean, low-level **pager**:
- Paginated on-disk file (fixed page size: **4 KiB**)
- Header validation (magic/version/page size/page count/flags)
- Robust I/O (`pread` with EINTR + short reads handling)
- Opaque API and modular design

## Quick start

```
# Generate fixtures and build tests
make run

# Or step-by-step
make fixtures
make tests
./tests/test_pager.bin

# Clean everything
make clean
```

## Repository layout

```
.
├── include/          # public headers (e.g., pager.h)
├── src/              # implementation (pager.c)
│   └── pager.c       # pager_open/read/close + small static helpers
├── tests/
│   ├── fixtures/
│   │   └── make_fixtures.c  # generates *.db test files
│   └── test_pager.c         # black-box tests
└── docs/             # design notes (optional)
```

## Public API

```c
// include/pager.h
typedef struct Pager Pager;

int      pager_open(const char* path, Pager** out);
int      pager_read(Pager* p, uint32_t page_no, void* out_page_buf);
void     pager_close(Pager* p);
size_t   pager_page_size(const Pager* p);
uint32_t pager_page_count(const Pager* p);
```

- Returns `0` on success, negative codes on error.
- The `Pager` type is opaque (implementation details hidden).

## On-disk format (v1)

- **Page size**: 4096 bytes (fixed in v1)
- **Page 0 (header)** — 20 bytes at the beginning:

| Offset | Size | Field        | Value (v1)        |
|------: |----: |--------------|-------------------|
| 0      | 4    | magic        | `"MDB1"`          |
| 4      | 4    | version      | `1`               |
| 8      | 4    | page_size    | `4096`            |
| 12     | 4    | page_count   | `>= 1`            |
| 16     | 4    | flags        | `0`               |

- All multi-byte integers are **little-endian** on disk.

## Testing

- Fixtures are created in `tests/fixtures/*.db` by `make_fixtures.c`.
- `tests/test_pager.c` performs black-box tests on `pager_open`, `pager_read`, and getters.

Typical run:
```
make run
# Expected:
# Fixtures created in tests/fixtures/
# All tests passed.
```

## Coding style highlights

- Single-exit `goto cleanup` pattern to avoid resource leaks.
- `read_full()` helper to guarantee exact reads (`pread` can be short and be interrupted).
- Header parsing done with `read_le_u32()`; no casting raw buffers into structs.
- Strict validation and overflow guards when computing file offsets.

## Roadmap

- [x] Pager open/read/close
- [x] Fixtures + tests
- [ ] Table page layout (TABLE_LEAF)
- [ ] Minimal INSERT/SELECT
- [ ] Secondary index / journaling (optional)

## License

See [LICENSE](LICENSE).
