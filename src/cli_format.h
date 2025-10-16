#ifndef CLI_FORMAT_H
#define CLI_FORMAT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ----- Generic field-spec parsing and pretty table for fixed-size records -----
// Spec grammar: name:off:len:type[,name:off:len:type...]
// Types: s (string, NUL-padded), hex, u8, u16 (LE), u32 (LE)

typedef enum { FT_STR, FT_HEX, FT_U8, FT_U16, FT_U32 } FieldType;

typedef struct {
  char      name[32];
  uint16_t  off;
  uint16_t  len;
  FieldType type;
  uint16_t  colw; // computed column width
} Field;

typedef struct {
  Field  f[16];   // up to 16 columns
  int    n;
} FieldSpec;

// Parse "name:off:len:type[, ...]" into FieldSpec.
// Mutates its input: pass a writable buffer, not a string literal.
int parse_spec(char* spec_in, FieldSpec* fs);

// Pretty table helpers for 128-byte records
void print_header_spec(const FieldSpec* fs);
void print_row_spec(uint32_t id, const FieldSpec* fs, const unsigned char rec[128]);
void print_footer_spec(const FieldSpec* fs, size_t rows);

// Scan callback + context for listf (to be used with tblmgr_scan)
typedef struct {
  const FieldSpec* fs;
  size_t*          counter;
} ListfCtx;

// Signature must match: int (*callback)(const void* record, uint32_t id, void* user_data)
int scan_cb_listf(const void* rec, uint32_t id, void* ud);

#ifdef __cplusplus
}
#endif

#endif // CLI_FORMAT_H