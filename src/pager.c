#include "pager.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <limits.h>

#define PAGE_SIZE 4096
#define HDR_MAGIC_OFF      0   // 4 bytes
#define HDR_VERSION_OFF    4   // u32 LE
#define HDR_PAGESIZE_OFF   8   // u32 LE
#define HDR_PAGECOUNT_OFF 12   // u32 LE
#define HDR_FLAGS_OFF     16   // u32 LE
#define HDR_SIZE 20
#define FILE_MAGIC      "MDB1"
#define FILE_MAGIC_LEN  4
#define FILE_VERSION    1u

enum {
    PAGER_OK = 0,
    PAGER_E_IO = -1,
    PAGER_E_MAGIC = -2,
    PAGER_E_VERSION = -3,
    PAGER_E_PAGESIZE = -4,
    PAGER_E_META = -5,
    PAGER_E_TRUNCATED = -6,
    PAGER_E_RANGE = -7,
    PAGER_E_INVAL = -8
};

struct Pager {
    int fd;
    size_t page_size;
    uint32_t page_count;
};


static int read_full(int fd, void* buf, size_t len, off_t base_offset) {
  size_t done = 0;
  ssize_t n = 0;

  while (done < len) {
    n = pread(fd, (char*)buf + done, len - done, base_offset + (off_t) done);

    if (n > 0) {
      done += n;
    } else if (n == 0) {
      return PAGER_E_IO;
    } else if (errno == EINTR) {
      continue;
    } else {
      return PAGER_E_IO;
    }
  }

  return PAGER_OK;
}

static inline uint32_t read_le_u32(const uint8_t *p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

int pager_open(const char* path, Pager** out) {
    int rc = PAGER_OK;
    int fd = -1;
    Pager *p = NULL;
    uint8_t header[HDR_SIZE];

    if (!path || !out)
        return PAGER_E_INVAL;

    *out = NULL;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        rc = PAGER_E_IO;
        goto cleanup;
    }

    p = calloc(1, sizeof *p);
    if (!p) {
        rc = PAGER_E_IO;
        goto cleanup;
    }

    rc = read_full(fd, header, HDR_SIZE, 0);
    if (rc != PAGER_OK)
        goto cleanup;

    uint32_t version    = read_le_u32(header + HDR_VERSION_OFF);
    uint32_t page_size  = read_le_u32(header + HDR_PAGESIZE_OFF);
    uint32_t page_count = read_le_u32(header + HDR_PAGECOUNT_OFF);
    uint32_t flags      = read_le_u32(header + HDR_FLAGS_OFF);

    if (memcmp(header + HDR_MAGIC_OFF, FILE_MAGIC, FILE_MAGIC_LEN) != 0) {
        rc = PAGER_E_MAGIC;
        goto cleanup;
    }

    if (version != FILE_VERSION) {
        rc = PAGER_E_VERSION;
        goto cleanup;
    }

    if (page_size != PAGE_SIZE) {
        rc = PAGER_E_PAGESIZE;
        goto cleanup;
    }

    if (page_count < 1) {
        rc = PAGER_E_META;
        goto cleanup;
    }

    if (flags != 0) {
        rc = PAGER_E_META;
        goto cleanup;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        rc = PAGER_E_IO;
        goto cleanup;
    }

    if ((uint64_t)page_count > (uint64_t)LLONG_MAX / (uint64_t)page_size) {
        rc = PAGER_E_META;
        goto cleanup;
    }

    off_t need = (off_t)page_size * (off_t)page_count;
    off_t filesize = st.st_size;

    if (filesize < need) {
        rc = PAGER_E_TRUNCATED;
        goto cleanup;
    }

    p->fd = fd;
    p->page_size = page_size;
    p->page_count = page_count;
    *out = p;
    return PAGER_OK;

cleanup:
    if (fd >= 0)
        close(fd);
    free(p);
    return rc;
}


int  pager_read(Pager* p, uint32_t page_no, void* out_page_buf) {
  if (!p || !out_page_buf)
    return PAGER_E_INVAL;

  if (page_no >= p->page_count)
    return PAGER_E_RANGE;

  if ((uint64_t) page_no > (UINT64_MAX / (uint64_t) p->page_size))
    return PAGER_E_META;

  off_t base = (off_t)page_no * (off_t)p->page_size;

  return read_full(p->fd, out_page_buf, p->page_size, base);
}

size_t pager_page_size(const Pager* p) {
  return p ? p->page_size : 0;
}

uint32_t pager_page_count(const Pager* p) {
  return p ? p->page_count : 0;
}

void pager_close(Pager* p) {
    if (!p) return;
    close(p->fd);
    free(p);
}
