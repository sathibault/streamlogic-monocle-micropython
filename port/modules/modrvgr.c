#include "py/runtime.h"

#if MICROPY_MALLOC_USES_ALLOCATED_SIZE
#define MFREE(ptr, sz) m_free(ptr, sz)
#else
#define MFREE(ptr, sz) m_free(ptr)
#endif

#define MAX_RUNS 128

#define MAX_DX 511
#define MAX_NLX 511
#define MAX_SPANX 511
#define MAX_CLRX 15
#define XFRAC 4


typedef struct rvgr_rect_obj_s {
  mp_obj_base_t base;
  uint8_t clr;
  uint16_t x, y, w, h;
} rvgr_rect_obj_t;

STATIC mp_obj_t rect_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
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

STATIC void rect_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
  (void)kind;
  
  rvgr_rect_obj_t * self = (rvgr_rect_obj_t *)MP_OBJ_TO_PTR(self_in);
  mp_printf(print, "Rect(%d+%d+%dx%d,c%d)", self->x, self->y, self->w, self->h, self->clr);
}

#if 0
STATIC const mp_rom_map_elem_t rect_locals_dict_table[] = {
  { MP_ROM_QSTR(MP_QSTR_time), MP_ROM_PTR(&rect_time_obj) },
};

STATIC MP_DEFINE_CONST_DICT(rect_locals_dict, rect_locals_dict_table);
#endif

#if 1
MP_DEFINE_CONST_OBJ_TYPE(
    rect_type,
    MP_QSTR_Rect,
    MP_TYPE_FLAG_NONE,
    make_new, (const void *)rect_make_new,
    print, (const void *)rect_print
    //    locals_dict, &rect_locals_dict
);
#else
const mp_obj_type_t rect_type = {
  .base = { &mp_type_type },
  .flags = MP_TYPE_FLAG_NONE,
  .name = MP_QSTR_Rect,
  .slot_index_make_new = 1,
  .slot_index_print = 2,
  .slots = { (const void *)rect_make_new, (const void *)rect_print } };
#endif

typedef struct iter_base_s {
  size_t size;
  bool (*nextLine)(void *, uint16_t*);
  bool (*nextRun)(void *, uint16_t, uint16_t*, uint16_t*, uint8_t*);
} iter_base_t;

typedef struct rect_iter_s {
  iter_base_t base;
  uint16_t y, y2;
  uint16_t x1, x2;
  uint16_t clr;
} rect_iter_t;
  
bool rect_next_line(void *arg, uint16_t* y) {
  rect_iter_t * iter = (rect_iter_t *)arg;
  *y = iter->y;
  return (*y <= iter->y2);
}

bool rect_next_run(void *arg, uint16_t y, uint16_t* x1, uint16_t *x2, uint8_t* clr) {
  rect_iter_t * iter = (rect_iter_t *)arg;
  if (iter->y == y) {
    *x1 = iter->x1;
    *x2 = iter->x2;
    *clr = iter->clr;
    iter->y += 1;
    return true;
  }
  return false;
};

STATIC iter_base_t *make_iter(mp_obj_t obj) {
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
  }
  return NULL;
}

STATIC void sort_runs(uint16_t *runs, uint8_t *clr, int n) {
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
    if (runs[ri-1] >= runs[ri])
      runs[ri-1] = runs[ri]-1;
  }
}

STATIC mp_obj_t generate(mp_obj_t addr_in, mp_obj_t list_in) {
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
	cmd = 0xa000|(x1<<XFRAC);
	curX = x1;
      } else {
	cmd = 0xf000|curY;
	curX = 0;
      }
      mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(cmd>>8));
      mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(cmd&0xff));
      for (i = 0; i < ri; i+=2) {
	dx = runs[i] - curX;
	while (dx > MAX_DX) {
	  cmd = 0x8000|(MAX_DX<<XFRAC);
	  mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(cmd>>8));
	  mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(cmd&0xff));
	  dx -= MAX_DX;
	}
	if (dx > 0) {
	  cmd = 0x8000|(dx<<XFRAC);
	  mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(cmd>>8));
	  mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(cmd&0xff));
	}
	
	s = runs[i+1] - runs[i];
	if (s > MAX_CLRX) {
	  cmd = (((uint16_t)clr[i>>1])<<8)|(MAX_CLRX<<XFRAC);
	  s -= MAX_CLRX;
	} else {
	  cmd = (((uint16_t)clr[i>>1])<<8)|(s<<XFRAC);
	  s = 0;
	}
	mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(cmd>>8));
	mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(cmd&0xff));
	while (s > MAX_SPANX) {
	  cmd = 0xc000|(MAX_SPANX<<XFRAC);
	  mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(cmd>>8));
	  mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(cmd&0xff));
	  s -= MAX_SPANX;
	}
	if (s > 0) {
	  cmd = 0xc000|(s<<XFRAC);
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


STATIC const mp_rom_map_elem_t module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_rvgr) },
    { MP_ROM_QSTR(MP_QSTR_Rect), MP_ROM_PTR(&rect_type) },
    { MP_ROM_QSTR(MP_QSTR_generate), MP_ROM_PTR(&generate_fun) },
};
STATIC MP_DEFINE_CONST_DICT(module_globals, module_globals_table);

const mp_obj_module_t rvgr_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_rvgr, rvgr_cmodule);
