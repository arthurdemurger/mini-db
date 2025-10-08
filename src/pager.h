#ifndef PAGER_H

#define PAGER_H

#include <stdint.h>
#include <stddef.h>

typedef struct Pager Pager;

int       pager_open(const char* path, Pager** out);
void      pager_close(Pager* p);
int       pager_read(Pager* p, uint32_t page_no, void* out_page_buf);
size_t    pager_page_size(const Pager* p);
uint32_t  pager_page_count(const Pager* p);


#endif // PAGER_H