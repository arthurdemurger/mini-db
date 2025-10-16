#include "cli_format.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// --- internal helpers ---------------------------------------------------------
static void trim(char* s) {
  char* p = s;
  while (*p == ' ' || *p == '\t') p++;
  if (p != s) memmove(s, p, strlen(p)+1);
  size_t n = strlen(s);
  while (n && (s[n-1]==' ' || s[n-1]=='\t')) s[--n] = 0;
}

static int parse_one_field(char* part, Field* out) {
  char* a = strtok(part, ":");
  char* b = a ? strtok(NULL, ":") : NULL;
  char* c = b ? strtok(NULL, ":") : NULL;
  char* d = c ? strtok(NULL, ":") : NULL;
  if (!a || !b || !c || !d) return -1;

  trim(a); trim(b); trim(c); trim(d);
  if (strlen(a) == 0 || strlen(a) >= sizeof out->name) return -1;

  strncpy(out->name, a, sizeof out->name);
  char* endptr = NULL;
  long off = strtol(b, &endptr, 10); if (*b=='\0' || *endptr) return -1;
  long len = strtol(c, &endptr, 10); if (*c=='\0' || *endptr) return -1;
  if (off < 0 || off > 65535 || len < 0 || len > 65535) return -1;
  out->off = (uint16_t)off;
  out->len = (uint16_t)len;

  if      (strcmp(d, "s")==0)   out->type = FT_STR;
  else if (strcmp(d, "hex")==0) out->type = FT_HEX;
  else if (strcmp(d, "u8")==0)  out->type = FT_U8;
  else if (strcmp(d, "u16")==0) out->type = FT_U16;
  else if (strcmp(d, "u32")==0) out->type = FT_U32;
  else return -1;

  uint16_t minw = (uint16_t)strlen(out->name);
  uint16_t w = 0;
  switch (out->type) {
    case FT_STR: w = (out->len > 30 ? 30 : out->len); break;
    case FT_HEX: w = (uint16_t)(out->len * 2); if (w > 32) w = 32; break;
    case FT_U8:  w = 3;  break;
    case FT_U16: w = 5;  break;
    case FT_U32: w = 10; break;
  }
  out->colw = (w < minw ? minw : w);
  if (out->colw > 40) out->colw = 40;
  return 0;
}

int parse_spec(char* spec_in, FieldSpec* fs) {
  fs->n = 0;
  char* s = spec_in;
  while (*s) {
    char* start = s;
    char* comma = strchr(s, ',');
    if (comma) *comma = 0;

    if (fs->n >= (int)(sizeof fs->f / sizeof fs->f[0])) return -1;
    char buf[128];
    strncpy(buf, start, sizeof buf - 1); buf[sizeof buf - 1] = 0;
    trim(buf);
    if (parse_one_field(buf, &fs->f[fs->n]) != 0) return -1;
    fs->n++;

    if (!comma) break;
    s = comma + 1;
  }
  return (fs->n > 0) ? 0 : -1;
}

static uint16_t rd_u16le(const unsigned char* p) { return (uint16_t)(p[0] | (p[1]<<8)); }
static uint32_t rd_u32le(const unsigned char* p) { return (uint32_t)(p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24)); }

static void render_field(const Field* f, const unsigned char rec[128], char* out, size_t outsz) {
  const unsigned char* base = rec + f->off;
  switch (f->type) {
    case FT_STR: {
      size_t n = 0;
      while (n < f->len && n + 1 < outsz) {
        unsigned char c = base[n];
        if (c == 0) break;
        out[n++] = (char)c;
      }
      out[n] = 0;
      for (ssize_t j = (ssize_t)n - 1; j >= 0; j--) {
        if (out[j] == ' ') out[j] = 0; else break;
      }
      if (strlen(out) > f->colw) out[f->colw] = 0;
    } break;
    case FT_HEX: {
      size_t k = 0;
      for (uint16_t i = 0; i < f->len && k + 2 < outsz; i++) {
        k += (size_t)snprintf(out + k, outsz - k, "%02x", base[i]);
        if (k >= (size_t)f->colw) break;
      }
      out[k] = 0;
    } break;
    case FT_U8:  snprintf(out, outsz, "%u", (unsigned)base[0]); break;
    case FT_U16: snprintf(out, outsz, "%u", (unsigned)rd_u16le(base)); break;
    case FT_U32: snprintf(out, outsz, "%u", (unsigned)rd_u32le(base)); break;
  }
}

static void print_hr(const FieldSpec* fs) {
  printf("+--------+");
  for (int i = 0; i < fs->n; i++) {
    for (uint16_t k = 0; k < fs->f[i].colw + 2; k++) putchar('-');
    putchar('+');
  }
  putchar('\n');
}

void print_header_spec(const FieldSpec* fs) {
  print_hr(fs);
  printf("| %-6s |", "ID");
  for (int i = 0; i < fs->n; i++) {
    printf(" %-*s |", fs->f[i].colw, fs->f[i].name);
  }
  putchar('\n');
  print_hr(fs);
}

void print_row_spec(uint32_t id, const FieldSpec* fs, const unsigned char rec[128]) {
  printf("| %6u |", id);
  char cell[256];
  for (int i = 0; i < fs->n; i++) {
    render_field(&fs->f[i], rec, cell, sizeof cell);
    printf(" %-*s |", fs->f[i].colw, cell);
  }
  putchar('\n');
}

void print_footer_spec(const FieldSpec* fs, size_t rows) {
  (void)fs;
  print_hr(fs);
  printf("%zu row(s)\n", rows);
}

int scan_cb_listf(const void* rec, uint32_t id, void* ud) {
  ListfCtx* ctx = (ListfCtx*)ud;
  print_row_spec(id, ctx->fs, (const unsigned char*)rec);
  (*ctx->counter)++;
  return 0;
}
