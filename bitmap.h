#ifndef BITMAP_H
#define BITMAP_H

#include <stdbool.h>


/* bitmap handle (opaque pointer) */
typedef void* bitmap_hdl;

/* allocate a bitmap */
bitmap_hdl bitmap_alloc(size_t numbits);

/* return the number of allocated bits */
size_t bitmap_numbits(bitmap_hdl hdl);

/* deallocate a bitmap */
void bitmap_dealloc(bitmap_hdl hdl);

/* reset all bits */
void bitmap_zero(bitmap_hdl hdl);

/* set all bits */
void bitmap_fill(bitmap_hdl hdl);

/* set a specific bit */
void bitmap_set(bitmap_hdl hdl, size_t pos);

/* reset a specific bit */
void bitmap_reset(bitmap_hdl hdl, size_t pos);

/* get the state of a specific bit */
bool bitmap_get(bitmap_hdl hdl, size_t pos);

/* copy nbits bits from src bitmap to dst bitmap */
void bitmap_copy(bitmap_hdl dst, const bitmap_hdl src, size_t nbits);

/* resize a bitmap. Bits which indices were valid before and are still valid remain unchanged */
void bitmap_resize(bitmap_hdl hdl, size_t nbits);

#endif
