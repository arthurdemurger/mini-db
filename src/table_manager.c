#include "table_manager.h"
#include <string.h>

int tblmgr_create(Pager* pager, uint32_t first_page_num) {
  if (!pager || first_page_num == 0)
    return TABLE_E_INVAL;

  while (first_page_num >= pager_page_count(pager)) {
    uint32_t out_page_no;
    int rc = pager_alloc_page(pager, &out_page_no);
    if (rc < 0) {
      return TABLE_E_INVAL;
    }
  }

  size_t page_sz = pager_page_size(pager);
  uint8_t *buf = calloc(1, page_sz);
  if (!buf) {
    return TABLE_E_INVAL;
  }
  int rc = pager_read(pager, first_page_num, buf);
  if (rc < 0) {
    free(buf);
    return TABLE_E_INVAL;
  }

  bool all_zero = true;
  for (int i = 0; i < page_sz; i++) {
    if (buf[i] != 0) {
      all_zero = false;
      break;
    }
  }

  if (all_zero) {
// empty page
/* Séquence (toutes les erreurs → libérer buf et retourner une TABLE_E_* appropriée) :

rc = tbl_init_leaf(buf, TABLE_RECORD_SIZE (128));

si rc != TABLE_OK → free(buf); return rc;

(défensif) rc = tbl_validate(buf);

si rc != TABLE_OK → free(buf); return rc;

rc = pager_write(pager, first_page_num, buf);

si rc != PAGER_OK → free(buf); return TABLE_E_INVAL;

free(buf); return TABLE_OK;
*/
  } else if (tbl_validate(buf) == TABLE_OK && tbl_get_used_count(buf) == 0) { // leaf but empty
/*
rc = tbl_validate(buf);

Si rc != TABLE_OK → free(buf); return rc;
(ex. TABLE_E_BADKIND, TABLE_E_LAYOUT, TABLE_E_BITMAP, etc.)

Si TABLE_OK, alors idempotence seulement si :

tbl_get_used_count(buf) == 0 et

tbl_get_next_page(buf) == 0

(facultatif) tbl_get_record_size(buf) == TABLE_RECORD_SIZE

Si ces conditions sont vraies → free(buf); return TABLE_OK;

Sinon (page déjà occupée/chaînée, ou record_size inattendu) :

Choisis ta politique : renvoyer TABLE_E_INVAL (ou TABLE_E_LAYOUT) et documenter que tblmgr_create refuse d’écraser une page non vide
*/
  }

  free(buf);
  return TABLE_E_INVAL;
}
