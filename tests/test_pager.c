// tests/test_all.c
// Black-box tests for pager_open / pager_read / getters / close.

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "pager.h"

// Local copy of header offsets for validation via pager_read(page 0).
#define FILE_MAGIC      "MDB1"
#define FILE_MAGIC_LEN   4
#define HDR_MAGIC_OFF    0

static void test_open_ok_and_read_header(void) {
    Pager* p = NULL;
    int rc = pager_open("tests/fixtures/valid.db", &p);
    assert(rc == 0 && "pager_open(valid.db) should succeed");
    assert(p != NULL);

    size_t ps = pager_page_size(p);
    uint32_t pc = pager_page_count(p);
    assert(ps == 4096 && "page size should be 4096 for v1");
    assert(pc == 3 && "page count should be 3");

    // Read page 0 and check magic
    uint8_t* buf = (uint8_t*)malloc(ps);
    assert(buf);
    rc = pager_read(p, 0, buf);
    assert(rc == 0 && "pager_read page 0 should succeed");
    assert(memcmp(buf + HDR_MAGIC_OFF, FILE_MAGIC, FILE_MAGIC_LEN) == 0 && "magic must be MDB1");

    free(buf);
    pager_close(p);
}

static void test_read_oob(void) {
    Pager* p = NULL;
    int rc = pager_open("tests/fixtures/valid.db", &p);
    assert(rc == 0 && p);

    size_t ps = pager_page_size(p);
    (void)ps;

    // Ask for page == page_count (out of range)
    uint32_t bad = pager_page_count(p);
    uint8_t* buf = (uint8_t*)malloc(ps);
    assert(buf);

    rc = pager_read(p, bad, buf);
    assert(rc < 0 && "reading out-of-range page should fail");

    free(buf);
    pager_close(p);
}

static void test_bad_magic(void) {
    Pager* p = NULL;
    int rc = pager_open("tests/fixtures/bad_magic.db", &p);
    assert(rc < 0 && "bad_magic should fail");
    assert(p == NULL);
}

static void test_bad_version(void) {
    Pager* p = NULL;
    int rc = pager_open("tests/fixtures/bad_version.db", &p);
    assert(rc < 0 && "bad_version should fail");
    assert(p == NULL);
}

static void test_bad_pagesize(void) {
    Pager* p = NULL;
    int rc = pager_open("tests/fixtures/bad_pagesize.db", &p);
    assert(rc < 0 && "bad_pagesize should fail");
    assert(p == NULL);
}

static void test_pagecount_zero(void) {
    Pager* p = NULL;
    int rc = pager_open("tests/fixtures/pagecount_zero.db", &p);
    assert(rc < 0 && "pagecount zero should fail");
    assert(p == NULL);
}

static void test_bad_flags(void) {
    Pager* p = NULL;
    int rc = pager_open("tests/fixtures/bad_flags.db", &p);
    assert(rc < 0 && "bad_flags should fail");
    assert(p == NULL);
}

static void test_truncated(void) {
    Pager* p = NULL;
    int rc = pager_open("tests/fixtures/truncated.db", &p);
    assert(rc < 0 && "truncated should fail");
    assert(p == NULL);
}

static void test_ok_extra(void) {
    // File can be larger than header's page_count * page_size.
    Pager* p = NULL;
    int rc = pager_open("tests/fixtures/ok_extra.db", &p);
    assert(rc == 0 && p);
    pager_close(p);
}

int main(void) {
    test_open_ok_and_read_header();
    test_read_oob();
    test_bad_magic();
    test_bad_version();
    test_bad_pagesize();
    test_pagecount_zero();
    test_bad_flags();
    test_truncated();
    test_ok_extra();
    printf("All tests passed.\n");
    return 0;
}
