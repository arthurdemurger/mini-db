#ifndef PAGER_H

#define PAGER_H

#include <stdint.h>
#include <stddef.h>

typedef enum PagerError {
  PAGER_OK = 0,
  PAGER_E_IO = -1,
  PAGER_E_MAGIC = -2,
  PAGER_E_VERSION = -3,
  PAGER_E_PAGESIZE = -4,
  PAGER_E_META = -5,
  PAGER_E_TRUNCATED = -6,
  PAGER_E_RANGE = -7,
  PAGER_E_INVAL = -8
} PagerError;

enum {
  PAGER_PAGE_SIZE = 4096,
  PAGER_HDR_SIZE  = 20
};

typedef struct Pager Pager;

int         pager_open(const char* path, Pager** out);
void        pager_close(Pager* p);
int         pager_read(const Pager* p, uint32_t page_no, void* out_page_buf);
size_t      pager_page_size(const Pager* p);
uint32_t    pager_page_count(const Pager* p);
const char* pager_errstr(int code);

#endif // PAGER_H