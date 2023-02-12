#include "py/runtime.h"

#if MICROPY_MALLOC_USES_ALLOCATED_SIZE
#define MFREE(ptr, sz) m_free(ptr, sz)
#else
#define MFREE(ptr, sz) m_free(ptr)
#endif

#define MAX_RUNS 128

#define XFRAC 4

#define MAX_DX 0x7fff // 9.4
#define MAX_NLX 0x7fff // 9.4
#define MAX_SPANX 0x7fff // 9.4
#define MAX_CLRX 0xff // 4.4
#define MIN_DX 0x10

#define XFX(x) (x<<XFRAC)
#define XFX_INT(x) (x>>XFRAC)
#define YFX(y) y
#define YFX_INT(y) y


#define ABS(a)		(((a)<0) ? -(a) : (a))
#define SIGN(x) ((x)>=0 ? 1 : -1)


//////////////////////////////////////// Utils

void list_minmax(uint16_t *pts, int n, uint16_t *minx_p, uint16_t *maxx_p, uint16_t *miny_p, uint16_t *maxy_p)

{
  *minx_p = 0xffffu;
  *miny_p = 0xffffu;
  *maxx_p = 0;
  *maxy_p = 0;
  for (int i = 0; i < n; i += 2) {
    uint16_t x = pts[i];
    uint16_t y = pts[i+1];
    if (x < *minx_p) *minx_p = x;
    if (x > *maxx_p) *maxx_p = x;
    if (y < *miny_p) *miny_p = y;
    if (y > *maxy_p) *maxy_p = y;
  }
}


//////////////////////////////////////// Edge

typedef struct edge {
  struct edge *next;
  int16_t yTop, yBot;
  int16_t xNowWhole, xNowNum, xNowDen, xNowDir;
  int16_t xNowNumStep;
} edge_t;

static void fill_edges(uint16_t *pts, int n, int y0, edge_t **edges)

{
  int i, j;
  int X1,Y1,X2,Y2,Y3;
  edge_t *e;

  i=0;
  do {
    i += 2;
    if (i == n)
      break;
    X1 = pts[i-2];
    Y1 = pts[i-1];
    X2 = pts[i];
    Y2 = pts[i+1];
    if (Y1==Y2)
      continue;   /* Skip horiz. edges */
    /* Find next vertex not level with p2 */
    j = i;
    do {
      j += 2;
      if (j == n)
	j = 2; // skip first point which is same as last
      Y3 = pts[j+1];
      if (Y2 != Y3)
	break;
    } while (1);
    e = (edge_t *) m_new(edge_t, 1);
    e->xNowNumStep = ABS(X1-X2);
    if (Y2 > Y1) {
      e->yTop = Y1;
      e->yBot = Y2;
      e->xNowWhole = X1;
      e->xNowDir = SIGN(X2 - X1);
      e->xNowDen = e->yBot - e->yTop;
      e->xNowNum = (e->xNowDen >> 1);
      if (Y3 > Y2)
	e->yBot--;
    } else {
      e->yTop = Y2;
      e->yBot = Y1;
      e->xNowWhole = X2;
      e->xNowDir = SIGN(X1 - X2);
      e->xNowDen = e->yBot - e->yTop;
      e->xNowNum = (e->xNowDen >> 1);
      if (Y3 < Y2) {
	e->yTop++;
	e->xNowNum += e->xNowNumStep;
	while (e->xNowNum >= e->xNowDen) {
	  e->xNowWhole += e->xNowDir;
	  e->xNowNum -= e->xNowDen;
	}
      }
    }
    e->next = edges[YFX_INT(e->yTop) - y0];
    edges[YFX_INT(e->yTop) - y0] = e;
  } while (1);
}


//////////////////////////////////////// Iter base

typedef struct iter_base_s {
  size_t size;
  bool (*nextLine)(void *, uint16_t*);
  bool (*nextRun)(void *, uint16_t, uint16_t*, uint16_t*, uint8_t*);
} iter_base_t;


//////////////////////////////////////// Rect

typedef struct rvgr_rect_obj_s {
  mp_obj_base_t base;
  uint8_t clr;
  uint16_t x, y, w, h;
} rvgr_rect_obj_t;

static mp_obj_t rect_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
  mp_arg_check_num(n_args, n_kw, 5, 5, false);

  rvgr_rect_obj_t *self = m_new_obj(rvgr_rect_obj_t);
  self->base.type = (mp_obj_type_t *)type;
  self->x = mp_obj_get_int(args[0]);
  self->y = mp_obj_get_int(args[1]);
  self->w = mp_obj_get_int(args[2]);
  self->h = mp_obj_get_int(args[3]);
  self->clr = mp_obj_get_int(args[4]);

  return MP_OBJ_FROM_PTR(self);
}

static void rect_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
  (void)kind;
  
  rvgr_rect_obj_t * self = (rvgr_rect_obj_t *)MP_OBJ_TO_PTR(self_in);
  mp_printf(print, "Rect(%d+%d+%dx%d,c%d)", self->x, self->y, self->w, self->h, self->clr);
}

#if 0
static const mp_rom_map_elem_t rect_locals_dict_table[] = {
  { MP_ROM_QSTR(MP_QSTR_time), MP_ROM_PTR(&rect_time_obj) },
};

static MP_DEFINE_CONST_DICT(rect_locals_dict, rect_locals_dict_table);
#endif

MP_DEFINE_CONST_OBJ_TYPE(
    rect_type,
    MP_QSTR_Rect,
    MP_TYPE_FLAG_NONE,
    make_new, (const void *)rect_make_new,
    print, (const void *)rect_print
    //    locals_dict, &rect_locals_dict
);

typedef struct rect_iter_s {
  iter_base_t base;
  uint16_t y, y2;
  uint16_t x1, x2;
  uint8_t clr;
} rect_iter_t;
  
bool rect_next_line(void *arg, uint16_t* y) {
  rect_iter_t * iter = (rect_iter_t *)arg;
  *y = iter->y;
  return (*y <= iter->y2);
}

bool rect_next_run(void *arg, uint16_t y, uint16_t* x1, uint16_t *x2, uint8_t* clr) {
  rect_iter_t * iter = (rect_iter_t *)arg;
  if (iter->y == y) {
    *x1 = XFX(iter->x1);
    *x2 = XFX(iter->x2);
    *clr = iter->clr;
    iter->y += 1;
    return true;
  }
  return false;
};


//////////////////////////////////////// Poly

typedef struct rvgr_poly_obj_s {
  mp_obj_base_t base;
  bool fill;
  uint8_t clr;
  uint16_t *pts;
  int n_pts;
} rvgr_poly_obj_t;

static mp_obj_t poly_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
  mp_arg_check_num(n_args, n_kw, 3, 3, false);

  rvgr_poly_obj_t *self = m_new_obj(rvgr_poly_obj_t);
  self->base.type = (mp_obj_type_t *)type;

  size_t list_len = 0;
  mp_obj_t *list = NULL;
  mp_obj_list_get(args[0], &list_len, &list);

  self->n_pts = 2*list_len + 2;
  self->pts = m_new(uint16_t, self->n_pts);

  int i, j;
  for (i = 0, j = 0; i < list_len; i++, j+=2) {
    size_t tpl_len;
    mp_obj_t *tpl;
    mp_obj_tuple_get(list[i], &tpl_len, &tpl);
    if (tpl_len==2) {
      self->pts[j] = XFX(mp_obj_get_int(tpl[0]));
      self->pts[j+1] = YFX(mp_obj_get_int(tpl[1]));
    } else {
      mp_raise_ValueError(MP_ERROR_TEXT("List element is not a pair"));
    }
  }
  self->pts[j++] = self->pts[0];
  self->pts[j++] = self->pts[1];

  self->fill = mp_obj_is_true(args[1]);
  self->clr = mp_obj_get_int(args[2]);

  return MP_OBJ_FROM_PTR(self);
}

static void poly_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
  (void)kind;
  
  rvgr_poly_obj_t * self = (rvgr_poly_obj_t *)MP_OBJ_TO_PTR(self_in);
  mp_printf(print, "Poly([");

  for (int i = 0; i < self->n_pts; i += 2) {
    if (i > 0) mp_printf(print, ",");
    mp_printf(print, "(%d,%d)", self->pts[i], self->pts[i+1]);
  }
  mp_printf(print, "],%d,c%d)", self->fill, self->clr);
}

#if 0
static const mp_rom_map_elem_t poly_locals_dict_table[] = {
  { MP_ROM_QSTR(MP_QSTR_time), MP_ROM_PTR(&poly_time_obj) },
};

static MP_DEFINE_CONST_DICT(poly_locals_dict, poly_locals_dict_table);
#endif

MP_DEFINE_CONST_OBJ_TYPE(
    poly_type,
    MP_QSTR_Poly,
    MP_TYPE_FLAG_NONE,
    make_new, (const void *)poly_make_new,
    print, (const void *)poly_print
    //    locals_dict, &poly_locals_dict
);

#define MAX_ACTIVE 8

typedef struct poly_iter_s {
  iter_base_t base;
  int idx; // next y not processed
  edge_t **edges;
  int n_edges;
  edge_t *active[MAX_ACTIVE];
  int16_t x_coords[MAX_ACTIVE];
  int n_active, cur;
  uint16_t y0, y;
  uint8_t clr;
} poly_iter_t;


static void poly_advance(poly_iter_t *iter, uint16_t curY) {
  int i, j;
  int subY = YFX(curY);
  // filter out finished edges
  for (i = 0, j = 0; i < iter->n_active; i++) {
    edge_t *e = iter->active[i];
    if (e->yBot >= subY)
      iter->active[j++] = e;
  }

  // push new edges starting
  int idx = curY-iter->y0;
  if (idx < iter->n_edges) {
    edge_t *cur = iter->edges[idx];
    while (cur != NULL && j < MAX_ACTIVE) {
      iter->active[j++] = cur;
      cur = cur->next;
    }
    iter->idx = idx;
  } else
    iter->idx = iter->n_edges;
  iter->n_active = j;
  iter->y = curY;
}

static void poly_get_active(poly_iter_t *iter) {
  int num_coords, i, j;

  poly_advance(iter, iter->y);
  while (iter->n_active == 0 && iter->idx < iter->n_edges) {
    iter->y += 1;
  }

  // sort and update
  num_coords = 0;
  for (i = 0; i < iter->n_active; i++) {
    edge_t *e = iter->active[i];
    int16_t x = e->xNowWhole;
    for (j = num_coords; j > 0 && iter->x_coords[j-1] > x; j--)
      iter->x_coords[j] = iter->x_coords[j-1];
    iter->x_coords[j] = x;
    num_coords++;
    e->xNowNum += e->xNowNumStep;
    while (e->xNowNum >= e->xNowDen) {
      e->xNowWhole += e->xNowDir;
      e->xNowNum -= e->xNowDen;
    }
  }

  iter->cur = 0;
}

static bool poly_next_line(void *arg, uint16_t* y) {
  poly_iter_t * iter = (poly_iter_t *)arg;
  *y = iter->y;
  return (iter->n_active > 0);
}

static bool poly_next_run(void *arg, uint16_t y, uint16_t* x1, uint16_t *x2, uint8_t* clr) {
  poly_iter_t * iter = (poly_iter_t *)arg;
  
  if (y == iter->y && iter->cur < iter->n_active) {
    *x1 = iter->x_coords[iter->cur];
    *x2 = iter->x_coords[iter->cur+1];
    *clr = iter->clr;
    iter->cur += 2;
    if (iter->cur >= iter->n_active) {
      iter->y += 1;
      poly_get_active(iter);
    }
    return true;
  }
  return false;
};


//////////////////////////////////////// Compile

static iter_base_t *make_iter(mp_obj_t obj) {
  uint16_t mnx, mxx, mny, mxy;

  const mp_obj_type_t * otype = mp_obj_get_type(obj);
  if (otype == &rect_type) {
    rvgr_rect_obj_t * rect = (rvgr_rect_obj_t *)MP_OBJ_TO_PTR(obj);
    rect_iter_t * iter = (rect_iter_t *)m_malloc(sizeof(rect_iter_t));
    iter->base.size = sizeof(rect_iter_t);
    iter->base.nextLine = rect_next_line;
    iter->base.nextRun = rect_next_run;
    iter->x1 = rect->x;
    iter->x2 = rect->x + rect->w-1;
    iter->y = rect->y;
    iter->y2 = rect->y + rect->h-1;
    iter->clr = rect->clr;
    return (iter_base_t *)iter;
  } else if (otype == &poly_type) {
    rvgr_poly_obj_t * poly = (rvgr_poly_obj_t *)MP_OBJ_TO_PTR(obj);
    poly_iter_t * iter = (poly_iter_t *)m_malloc(sizeof(poly_iter_t));
    iter->base.size = sizeof(poly_iter_t);
    iter->base.nextLine = poly_next_line;
    iter->base.nextRun = poly_next_run;
    list_minmax(poly->pts, poly->n_pts, &mnx, &mxx, &mny, &mxy);
    iter->y0 = mny;
    iter->n_edges = mxy-mny+1;
    iter->edges = (edge_t **)m_malloc(sizeof(edge_t *) * iter->n_edges);
    for (int i = 0; i < iter->n_edges; i++)
      iter->edges[i] = NULL;
    fill_edges(poly->pts, poly->n_pts, mny, iter->edges);
    iter->idx = 0;
    iter->n_active = 0;
    iter->y = mny;
    poly_get_active(iter);
    iter->clr = poly->clr;
    return (iter_base_t *)iter;
  }
  return NULL;
}

static void sort_runs(uint16_t *runs, uint8_t *clr, int n) {
  uint8_t c;
  uint16_t x1, x2;
  
  for (int i = 0; i < n; i++) {
    for (int j = n-1; j > i; j--) {
      int ri = j<<1;
      if (runs[ri-2] > runs[ri]) {
	c = clr[j-1];
	x1 = runs[ri-2];
	x2 = runs[ri-1];
	runs[ri-2] = runs[ri];
	runs[ri-1] = runs[ri+1];
	clr[j-1] = clr[j];
	runs[ri] = x1;
	runs[ri+1] = x2;
	clr[j] = c;
      }
    }
  }
  for (int i = 1; i < n; i++) {
    int ri = i<<1;
    int dx = (int)runs[ri]-(int)runs[ri-1];
    if (dx < MIN_DX) {
      runs[ri] = runs[ri-1]+1;
      if (runs[ri] > runs[ri+1])
	runs[ri] = runs[ri+1]; // will be discarded later
    }
  }
}

static mp_obj_t generate(mp_obj_t addr_in, mp_obj_t list_in) {
  uint16_t addr = mp_obj_get_int(addr_in);

  size_t list_len = 0;
  mp_obj_t *list = NULL;
  mp_obj_list_get(list_in, &list_len, &list);
  int len = (int)list_len;

  uint16_t cmd;
  uint16_t curY, y, prevY;
  uint16_t x1, x2, dx, s, curX;
  uint8_t c;
  int i, ri;

  iter_base_t ** iters =(iter_base_t **)m_malloc(len * sizeof(iter_base_t*));
  for (int i = 0; i < len; i++)
    iters[i] = make_iter(list[i]);

  uint16_t * runs = (uint16_t *)m_malloc(2 * MAX_RUNS * sizeof(uint16_t));
  uint8_t * clr = (uint8_t *)m_malloc(MAX_RUNS * sizeof(uint8_t));

  mp_obj_t return_list = mp_obj_new_list(0, NULL);

  mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(addr>>8));
  mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(addr&0xff));
  
  prevY = 0xffff;
  do {
    // find next closest line
    curY = 0xffff;
    for (i = 0; i < len; i++) {
      if (iters[i] != NULL) {
	if (iters[i]->nextLine(iters[i], &y)) {
	  if (y < curY)
	    curY = y;
	} else {
	  MFREE(iters[i], iters[i]->size);
	  iters[i] = NULL;
	}
      }
    }
    if (curY == 0xffff) break;

    // collect runs on this line
    ri = 0;
    for (i = 0; i < len; i++) {
      if (iters[i] != NULL) {
	while (iters[i]->nextRun(iters[i], curY, &x1, &x2, &c) && x2 > x1) {
	  if ((ri>>1) < MAX_RUNS) {
	    clr[ri>>1] = c;
	    runs[ri++] = x1;
	    runs[ri++] = x2;
	  }
	}
      }
    }

    if (ri > 0) {
      sort_runs(runs, clr, ri>>1);

      x1 = runs[0];
      if (curY == (prevY+1) && x1 <= MAX_NLX) {
	cmd = 0xa000|x1;
	curX = x1;
      } else {
	cmd = 0xf000|curY;
	curX = 0;
      }
      mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(cmd>>8));
      mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(cmd&0xff));
      for (i = 0; i < ri; i+=2) {
	s = runs[i+1] - runs[i];
	if (s < MIN_DX)
	  continue;

	dx = runs[i] - curX;
	while (dx > MAX_DX) {
	  cmd = 0x8000|MAX_DX;
	  mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(cmd>>8));
	  mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(cmd&0xff));
	  dx -= MAX_DX;
	}
	if (dx > 0) {
	  cmd = 0x8000|dx;
	  mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(cmd>>8));
	  mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(cmd&0xff));
	}
	
	if (s > MAX_CLRX) {
	  cmd = (((uint16_t)clr[i>>1])<<8)|MAX_CLRX;
	  s -= MAX_CLRX;
	} else {
	  cmd = (((uint16_t)clr[i>>1])<<8)|s;
	  s = 0;
	}
	mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(cmd>>8));
	mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(cmd&0xff));
	while (s > MAX_SPANX) {
	  cmd = 0xc000|MAX_SPANX;
	  mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(cmd>>8));
	  mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(cmd&0xff));
	  s -= MAX_SPANX;
	}
	if (s > 0) {
	  cmd = 0xc000|s;
	  mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(cmd>>8));
	  mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(cmd&0xff));
	}
	
	curX = runs[i+1] + 1;
      }
    }
    
    prevY = curY;
  } while (true);

  mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(0xff));
  mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(0xff));

  for (i = 0; i < len; i++) {
    if (iters[i] != NULL) {
      MFREE(iters[i], iters[i]->size);
    }
  }

  MFREE(clr, MAX_RUNS * sizeof(uint8_t));
  MFREE(runs, 2 * MAX_RUNS * sizeof(uint16_t));
  MFREE(iters, len * sizeof(iter_base_t*));
  
  return MP_OBJ_FROM_PTR(return_list);
}

const mp_obj_fun_builtin_fixed_t generate_fun = {{&mp_type_fun_builtin_2}, {._2 = &generate}};


static const mp_rom_map_elem_t module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_rvgr) },
    { MP_ROM_QSTR(MP_QSTR_Rect), MP_ROM_PTR(&rect_type) },
    { MP_ROM_QSTR(MP_QSTR_Poly), MP_ROM_PTR(&poly_type) },
    { MP_ROM_QSTR(MP_QSTR_generate), MP_ROM_PTR(&generate_fun) },
};
static MP_DEFINE_CONST_DICT(module_globals, module_globals_table);

const mp_obj_module_t rvgr_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_rvgr, rvgr_cmodule);
