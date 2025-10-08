// tests/mkfixtures.c
// Build minimal .db fixtures with a 20-byte LE header and page-aligned file size.

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define FILE_HEADER_SIZE 20
#define FILE_MAGIC      "MDB1"
#define FILE_MAGIC_LEN   4
#define FILE_VERSION     1u

// Header offsets (explicit for readability)
#define HDR_MAGIC_OFF      0  // 4 bytes
#define HDR_VERSION_OFF    4  // u32 LE
#define HDR_PAGESIZE_OFF   8  // u32 LE
#define HDR_PAGECOUNT_OFF 12  // u32 LE
#define HDR_FLAGS_OFF     16  // u32 LE

static inline void write_le_u32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v      );
    p[1] = (uint8_t)(v >>  8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static int ensure_dir(const char* path) {
    // Create directory if missing; ignore if it already exists.
    if (mkdir(path, 0755) == 0) return 0;
    if (errno == EEXIST) return 0;
    return -1;
}

static int write_header(int fd,
                        const char* magic,
                        uint32_t version,
                        uint32_t page_size,
                        uint32_t page_count,
                        uint32_t flags) {
    // Build a full page-0 buffer (zeroed) with a 20-byte header at the start.
    uint8_t* page0 = (uint8_t*)calloc(1, PAGE_SIZE);
    if (!page0) return -1;

    // Magic
    memcpy(page0 + HDR_MAGIC_OFF, magic, FILE_MAGIC_LEN);
    // Version
    write_le_u32(page0 + HDR_VERSION_OFF, version);
    // Page size
    write_le_u32(page0 + HDR_PAGESIZE_OFF, page_size);
    // Page count
    write_le_u32(page0 + HDR_PAGECOUNT_OFF, page_count);
    // Flags
    write_le_u32(page0 + HDR_FLAGS_OFF, flags);

    // Write the first page
    ssize_t done = 0;
    while (done < PAGE_SIZE) {
        ssize_t n = pwrite(fd, page0 + done, PAGE_SIZE - done, done);
        if (n < 0) {
            if (errno == EINTR) continue;
            free(page0);
            return -1;
        }
        if (n == 0) { free(page0); return -1; }
        done += n;
    }

    free(page0);
    return 0;
}

static int create_db(const char* path,
                     const char* magic,
                     uint32_t version,
                     uint32_t page_size,
                     uint32_t page_count,
                     uint32_t flags,
                     bool truncate_to_exact_pages) {
    // Create file, write header page, and size the file.
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;

    // Always write page 0 with header
    if (write_header(fd, magic, version, page_size, page_count, flags) != 0) {
        close(fd);
        return -1;
    }

    // Grow or truncate to desired size:
    // - If truncate_to_exact_pages == true: size = page_count * PAGE_SIZE
    // - If false: you can choose a smaller size to simulate truncation
    off_t wanted = (off_t)page_count * (off_t)PAGE_SIZE;
    if (ftruncate(fd, truncate_to_exact_pages ? wanted : (wanted - PAGE_SIZE)) != 0) {
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int main(void) {
    // Ensure fixtures directory exists
    if (ensure_dir("tests") != 0) { perror("mkdir tests"); return 1; }
    if (ensure_dir("tests/fixtures") != 0) { perror("mkdir tests/fixtures"); return 1; }

    // valid.db: correct header, 3 pages, exact size
    if (create_db("tests/fixtures/valid.db", FILE_MAGIC, FILE_VERSION, PAGE_SIZE, 3, 0, true) != 0) {
        fprintf(stderr, "Failed to create valid.db\n");
        return 1;
    }

    // bad_magic.db: wrong magic, 3 pages, exact size
    if (create_db("tests/fixtures/bad_magic.db", "XXXX", FILE_VERSION, PAGE_SIZE, 3, 0, true) != 0) {
        fprintf(stderr, "Failed to create bad_magic.db\n");
        return 1;
    }

    // bad_version.db: version = 2, 3 pages
    if (create_db("tests/fixtures/bad_version.db", FILE_MAGIC, 2, PAGE_SIZE, 3, 0, true) != 0) {
        fprintf(stderr, "Failed to create bad_version.db\n");
        return 1;
    }

    // bad_pagesize.db: page_size = 2048 (pager expects 4096)
    if (create_db("tests/fixtures/bad_pagesize.db", FILE_MAGIC, FILE_VERSION, 2048u, 3, 0, true) != 0) {
        fprintf(stderr, "Failed to create bad_pagesize.db\n");
        return 1;
    }

    // pagecount_zero.db: page_count = 0
    if (create_db("tests/fixtures/pagecount_zero.db", FILE_MAGIC, FILE_VERSION, PAGE_SIZE, 0, 0, true) != 0) {
        fprintf(stderr, "Failed to create pagecount_zero.db\n");
        return 1;
    }

    // bad_flags.db: flags != 0 (e.g., 1)
    if (create_db("tests/fixtures/bad_flags.db", FILE_MAGIC, FILE_VERSION, PAGE_SIZE, 3, 1, true) != 0) {
        fprintf(stderr, "Failed to create bad_flags.db\n");
        return 1;
    }

    // truncated.db: header says 3 pages, file sized to only 2 pages
    if (create_db("tests/fixtures/truncated.db", FILE_MAGIC, FILE_VERSION, PAGE_SIZE, 3, 0, false) != 0) {
        fprintf(stderr, "Failed to create truncated.db\n");
        return 1;
    }

    // ok_extra.db: header says 3 pages, file sized to 4 pages (allowed)
    // Demonstrates filesize >= need and a multiple of PAGE_SIZE.
    if (create_db("tests/fixtures/ok_extra.db", FILE_MAGIC, FILE_VERSION, PAGE_SIZE, 3, 0, true) != 0) {
        fprintf(stderr, "Failed to create ok_extra.db\n");
        return 1;
    } else {
        // Expand by one more page (simulate preallocation)
        int fd = open("tests/fixtures/ok_extra.db", O_WRONLY);
        if (fd >= 0) {
            off_t four_pages = (off_t)4 * (off_t)PAGE_SIZE;
            (void)ftruncate(fd, four_pages);
            close(fd);
        }
    }

    printf("Fixtures created in tests/fixtures/\n");
    return 0;
}
