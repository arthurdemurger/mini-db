// tests/test_table.c
// Unit tests for table leaf page: init, validate, bitmap invariants, find_free.

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/table.h"
#include "../src/endian_util.h"

#ifndef TABLE_PAGE_SIZE
#define TABLE_PAGE_SIZE 4096
#endif

// Helpers (local to tests)
static inline uint8_t* page_bitmap(uint8_t* page) {
    return page + TABLE_HDR_SIZE;
}

static inline uint16_t hdr_get_u16(const uint8_t* page, size_t off) {
    return read_le_u16(page + off);
}
static inline void hdr_set_u16(uint8_t* page, size_t off, uint16_t v) {
    write_le_u16(page + off, v);
}

static void test_init_and_validate_ok(void) {
    uint8_t page[TABLE_PAGE_SIZE];
    int rc = tbl_init_leaf(page, TABLE_RECORD_SIZE);
    assert(rc == TABLE_OK && "tbl_init_leaf should succeed for V1");
    assert(tbl_validate(page) == TABLE_OK && "validate must pass after init");

    // Capacity should be >0 (spec V1: 31 for record_size=128)
    uint16_t cap = hdr_get_u16(page, TABLE_HDR_CAPACITY_OFF);
    assert(cap >= 1);
}

static void test_find_free_basic(void) {
    uint8_t page[TABLE_PAGE_SIZE];
    assert(tbl_init_leaf(page, TABLE_RECORD_SIZE) == TABLE_OK);

    // On empty page, first free is 0
    int idx = tbl_slot_find_free(page);
    assert(idx == 0 && "first free slot on empty page must be 0");

    // Mark bit0 used: set LSB of first bitmap byte; keep header's used_count in sync
    uint8_t* bm = page_bitmap(page);
    bm[0] |= 0x01;
    uint16_t used = hdr_get_u16(page, TABLE_HDR_USED_COUNT_OFF);
    hdr_set_u16(page, TABLE_HDR_USED_COUNT_OFF, (uint16_t)(used + 1));

    // Now first free should be 1
    idx = tbl_slot_find_free(page);
    assert(idx == 1 && "first free should move to 1 after taking slot 0");
}

static void test_validate_popcount_mismatch(void) {
    uint8_t page[TABLE_PAGE_SIZE];
    assert(tbl_init_leaf(page, TABLE_RECORD_SIZE) == TABLE_OK);

    // Set two bits in bitmap but lie in used_count (=1)
    uint8_t* bm = page_bitmap(page);
    bm[0] |= 0x03; // bits 0 and 1
    hdr_set_u16(page, TABLE_HDR_USED_COUNT_OFF, 1);

    int rc = tbl_validate(page);
    assert(rc == TABLE_E_BITMAP && "validate must fail when popcount != used_count");
}

static void test_validate_last_byte_extra_bits(void) {
    uint8_t page[TABLE_PAGE_SIZE];
    assert(tbl_init_leaf(page, TABLE_RECORD_SIZE) == TABLE_OK);

    // Compute capacity & bitmap size
    uint16_t cap = hdr_get_u16(page, TABLE_HDR_CAPACITY_OFF);
    size_t bm_bytes = (cap + 7u) / 8u;
    uint16_t valid_bits_last = (uint16_t)(cap & 7u);

    if (valid_bits_last != 0) {
        // Violate invariant: set a "beyond-capacity" bit (MSB side) in last byte
        uint8_t* bm = page_bitmap(page);
        // Example: for valid_bits_last=7, set bit7 (10000000)
        bm[bm_bytes - 1] |= (uint8_t)(0xFFu << valid_bits_last);
        // Keep used_count unchanged → validate should flag E_BITMAP due to invalid MSB bits
        int rc = tbl_validate(page);
        assert(rc == TABLE_E_BITMAP && "invalid MSB beyond capacity must fail validation");
    } else {
        // If cap is multiple of 8, nothing to test here
        assert(1 && "cap multiple of 8: skip extra-bits test");
    }
}

static void test_find_free_full_page(void) {
    uint8_t page[TABLE_PAGE_SIZE];
    assert(tbl_init_leaf(page, TABLE_RECORD_SIZE) == TABLE_OK);

    uint16_t cap = hdr_get_u16(page, TABLE_HDR_CAPACITY_OFF);
    size_t bm_bytes = (cap + 7u) / 8u;

    // Fill all valid bits to 1; keep "beyond-capacity" bits at 0
    uint8_t* bm = page_bitmap(page);
    memset(bm, 0xFF, bm_bytes);
    uint16_t valid_bits_last = (uint16_t)(cap & 7u);
    if (valid_bits_last != 0) {
        // Clear MSB side beyond capacity
        uint8_t keep_mask = (uint8_t)((1u << valid_bits_last) - 1u);
        bm[bm_bytes - 1] &= keep_mask;
    }

    // Update header's used_count = cap (to remain consistent)
    hdr_set_u16(page, TABLE_HDR_USED_COUNT_OFF, cap);

    // Validate OK then find_free must return -1
    assert(tbl_validate(page) == TABLE_OK);
    int idx = tbl_slot_find_free(page);
    assert(idx == -1 && "no free slot when all valid bits are 1");
}

int main(void) {
    test_init_and_validate_ok();
    test_find_free_basic();
    test_validate_popcount_mismatch();
    test_validate_last_byte_extra_bits();
    test_find_free_full_page();
    printf("table.c tests passed.\n");
    return 0;
}
