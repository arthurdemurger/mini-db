#include "pager.h"
#include "endian_util.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <limits.h>

// ─────────────────────────────────────────────────────────────────────────────
// Defines (on-disk header layout)
// ─────────────────────────────────────────────────────────────────────────────
#define HDR_MAGIC_OFF      0   // 4 bytes
#define HDR_VERSION_OFF    4   // u32 LE
#define HDR_PAGESIZE_OFF   8   // u32 LE
#define HDR_PAGECOUNT_OFF 12   // u32 LE
#define HDR_FLAGS_OFF     16   // u32 LE
#define FILE_MAGIC      "MDB1"
#define FILE_MAGIC_LEN  4
#define FILE_VERSION    1u

// ─────────────────────────────────────────────────────────────────────────────
// Private type
// ─────────────────────────────────────────────────────────────────────────────
struct Pager {
    int fd;
    size_t page_size;
    uint32_t page_count;
};

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────
/**
 * @brief Read exactly `len` bytes at `base_offset` using pread.
 *        Handles EINTR and short reads safely.
 */
static int read_full(int fd, void* buf, size_t len, off_t base_offset) {
  size_t done = 0;
  ssize_t n = 0;

  while (done < len) {
    n = pread(fd, (char*)buf + done, len - done, base_offset + (off_t) done);

    if (n > 0) {
      done += n;
    } else if (n == 0) {
      return PAGER_E_IO;
    } else if (errno == EINTR || errno == EAGAIN) {
      continue;
    } else {
      return PAGER_E_IO;
    }
  }

  return PAGER_OK;
}

/**
 * @brief Write exactly `len` bytes at `base_offset` using pwrite.
 *        Handles EINTR and short writes safely.
 */
static int write_full(int fd, const void* buf, size_t len, off_t base_offset) {
  size_t done = 0;
  ssize_t n = 0;

  while (done < len) {
    n = pwrite(fd, (const char*) buf + done, len - done, base_offset + (off_t) done);

    if (n > 0) {
      done += n;
    } else if (n == 0) {
      return PAGER_E_IO;
    } else if (errno == EINTR || errno == EAGAIN) {
      continue;
    } else {
      return PAGER_E_IO;
    }
  }

  return PAGER_OK;
}

/**
 * @brief Validate the 20-byte on-disk header and extract fields.
 *        Checks magic/version/page_size/page_count/flags.
 */
static int validate_header(const uint8_t hdr[PAGER_HDR_SIZE],
                           uint32_t* out_version,
                           uint32_t* out_page_size,
                           uint32_t* out_page_count,
                           uint32_t* out_flags) {
  if (memcmp(hdr + HDR_MAGIC_OFF, FILE_MAGIC, FILE_MAGIC_LEN) != 0)
    return PAGER_E_MAGIC;

  const uint32_t version    = read_le_u32(hdr + HDR_VERSION_OFF);
  const uint32_t page_size  = read_le_u32(hdr + HDR_PAGESIZE_OFF);
  const uint32_t page_count = read_le_u32(hdr + HDR_PAGECOUNT_OFF);
  const uint32_t flags      = read_le_u32(hdr + HDR_FLAGS_OFF);

  if (version != FILE_VERSION)         return PAGER_E_VERSION;
  if (page_size != PAGER_PAGE_SIZE)    return PAGER_E_PAGESIZE;
  if (page_count < 1)                  return PAGER_E_META;
  if (flags != 0)                      return PAGER_E_META;

  if (out_version)    *out_version    = version;
  if (out_page_size)  *out_page_size  = page_size;
  if (out_page_count) *out_page_count = page_count;
  if (out_flags)      *out_flags      = flags;
  return PAGER_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Open and validate a MiniDB file, returning an opaque Pager handle.
 */
int pager_open(const char* path, Pager** out) {
    int rc = PAGER_OK;
    int fd = -1;
    Pager *p = NULL;
    uint8_t header[PAGER_HDR_SIZE];

    if (!path || !out)
        return PAGER_E_INVAL;

    *out = NULL;

    fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        rc = PAGER_E_IO;
        goto cleanup;
    }

    {
      struct stat st0;
      if (fstat(fd, &st0) < 0) {
        rc = PAGER_E_IO;
        goto cleanup;
      }
      if (st0.st_size == 0) {
        uint8_t init_hdr[PAGER_HDR_SIZE];
        memcpy(init_hdr + HDR_MAGIC_OFF, FILE_MAGIC, FILE_MAGIC_LEN);
        write_le_u32(init_hdr + HDR_VERSION_OFF, FILE_VERSION);
        write_le_u32(init_hdr + HDR_PAGESIZE_OFF, PAGER_PAGE_SIZE);
        write_le_u32(init_hdr + HDR_PAGECOUNT_OFF, 1u);
        write_le_u32(init_hdr + HDR_FLAGS_OFF, 0u);

        rc = write_full(fd, init_hdr, PAGER_HDR_SIZE, 0);
        if (rc != PAGER_OK) {
          goto cleanup;
        }

        if (ftruncate(fd, (off_t)PAGER_PAGE_SIZE) != 0) {
          rc = PAGER_E_IO;
          goto cleanup;
        }
      }
    }

    rc = read_full(fd, header, PAGER_HDR_SIZE, 0);
    if (rc != PAGER_OK)
        goto cleanup;

    uint32_t version    = 0;
    uint32_t page_size  = 0;
    uint32_t page_count = 0;
    uint32_t flags      = 0;

    if ((rc = validate_header(header, &version, &page_size, &page_count, &flags)) != PAGER_OK)
      goto cleanup;

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

    p = calloc(1, sizeof *p);
    if (!p) {
        rc = PAGER_E_IO;
        goto cleanup;
    }
    p->fd = fd;
    fd = -1; // ownership transferred
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

/**
 * @brief Read a full page by number into out_page_buf.
 *        Guards against out-of-range and arithmetic overflow.
 */
int  pager_read(const Pager* p, uint32_t page_no, void* out_page_buf) {
  if (!p || !out_page_buf)
    return PAGER_E_INVAL;

  if (page_no >= p->page_count)
    return PAGER_E_RANGE;

  if ((uint64_t) page_no > (UINT64_MAX / (uint64_t) p->page_size))
    return PAGER_E_META;

  off_t base = (off_t)page_no * (off_t)p->page_size;

  return read_full(p->fd, out_page_buf, p->page_size, base);
}

int pager_write(const Pager* p, uint32_t page_no, const void* page_buf) {
  if (!p || !page_buf)
    return PAGER_E_INVAL;

      if ((uint64_t) page_no > (UINT64_MAX / (uint64_t) p->page_size))
    return PAGER_E_META;

  off_t base = (off_t)page_no * (off_t)p->page_size;

  if (page_no >= p->page_count)
    return PAGER_E_RANGE;

  return write_full(p->fd, page_buf, p->page_size, base);
}

int pager_alloc_page(Pager* p, uint32_t* out_page_no){
  if (!p || !out_page_no)
    return PAGER_E_INVAL;

  if (p->page_count == UINT32_MAX)
    return PAGER_E_META;

  uint32_t new_no = p->page_count;

  uint64_t result  = (uint64_t)new_no * (uint64_t) p->page_size;

  if (result > (uint64_t)INT64_MAX ||
      result + p->page_size > (uint64_t) INT64_MAX)
    return PAGER_E_META;

  uint8_t *buffer = calloc(1, sizeof(uint8_t) * p->page_size);
  if (!buffer)
    return PAGER_E_IO;

  off_t base_offset = (off_t)new_no * (off_t)p->page_size;
  uint8_t hdr_count[4];

  int rc = write_full(p->fd, buffer, p->page_size, base_offset);
  if (rc != PAGER_OK) {
    free(buffer);
    return rc;
  }
  p->page_count = p->page_count + 1;
  write_le_u32(hdr_count, p->page_count);

  rc = write_full(p->fd, hdr_count, 4, HDR_PAGECOUNT_OFF);
  if (rc != PAGER_OK) {
    free(buffer);
    return rc;
  }

  free(buffer);

  *out_page_no = new_no;
  return PAGER_OK;
}


/**
 * @brief Return the page size used by this Pager.
 */
size_t pager_page_size(const Pager* p) {
  return p ? p->page_size : 0;
}


/**
 * @brief Return the number of pages in the file.
 */
uint32_t pager_page_count(const Pager* p) {
  return p ? p->page_count : 0;
}

/**
 * @brief Close the Pager and free resources.
 */
void pager_close(Pager* p) {
    if (!p) return;
    close(p->fd);
    free(p);
}

// ─────────────────────────────────────────────────────────────────────────────
// Utilities
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Convert an error code to a human-readable string.
 */
const char* pager_errstr(int code) {
  switch (code) {
    case PAGER_OK:         return "ok";
    case PAGER_E_IO:       return "io";
    case PAGER_E_MAGIC:    return "bad_magic";
    case PAGER_E_VERSION:  return "bad_version";
    case PAGER_E_PAGESIZE: return "bad_pagesize";
    case PAGER_E_META:     return "bad_metadata";
    case PAGER_E_TRUNCATED:return "truncated_file";
    case PAGER_E_RANGE:    return "page_out_of_range";
    case PAGER_E_INVAL:    return "invalid_argument";
    default:               return "unknown";
  }
}
