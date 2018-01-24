#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <asm/types.h>
#include "bitmap.h"

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

#define BIT_MASK(nr)		(1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)		((nr) / BITS_PER_LONG)
#define BITS_PER_BYTE		8
#define BITS_TO_LONGS(nr)	DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long))

#define BITS_PER_LONG __BITS_PER_LONG

#define BITMAP_FIRST_WORD_MASK(start) (~0UL << ((start) & (BITS_PER_LONG - 1)))
#define BITMAP_LAST_WORD_MASK(nbits) (~0UL >> (-(nbits) & (BITS_PER_LONG - 1)))

typedef struct bitmap_ {
	size_t nbits;
	unsigned long *bits;
} bitmap_t;

bitmap_hdl bitmap_alloc(size_t nbits)
{
	bitmap_t *bm = NULL;
	if (nbits) {
		bm = (bitmap_t*) malloc(sizeof(bitmap_t));
		bm->nbits = nbits;
		size_t nbytes = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
		bm->bits = (unsigned long*) malloc(nbytes);
	}
	return bm;
}

size_t bitmap_numbits(bitmap_hdl hdl)
{
	bitmap_t *bm = (bitmap_t*) hdl;
	if (bm)
		return bm->nbits;
	return 0;
}

void bitmap_dealloc(bitmap_hdl hdl)
{
	bitmap_t *bm = (bitmap_t*) hdl;
	if (bm) {
		free(bm->bits);
		bm->nbits = 0;
	}
}

void bitmap_zero(bitmap_hdl hdl)
{
	bitmap_t *bm = (bitmap_t*) hdl;
	unsigned int len = BITS_TO_LONGS(bm->nbits) * sizeof(unsigned long);
	memset(bm->bits, 0, len);
}

void bitmap_fill(bitmap_hdl hdl)
{
	bitmap_t *bm = (bitmap_t*) hdl;
	unsigned int nlongs = BITS_TO_LONGS(bm->nbits);
	unsigned int len = (nlongs - 1) * sizeof(unsigned long);
	memset(bm->bits, 0xff,  len);
	bm->bits[nlongs - 1] = BITMAP_LAST_WORD_MASK(bm->nbits);
}

void bitmap_set(bitmap_hdl hdl, size_t pos)
{
	bitmap_t *bm = (bitmap_t*) hdl;
	unsigned long *p = ((unsigned long *)bm->bits) + BIT_WORD(pos);
	*p |= BIT_MASK(pos);
}

void bitmap_reset(bitmap_hdl hdl, size_t pos)
{
	bitmap_t *bm = (bitmap_t*) hdl;
	unsigned long *p = ((unsigned long *)bm->bits) + BIT_WORD(pos);
	*p &= ~(BIT_MASK(pos));
}

bool bitmap_get(bitmap_hdl hdl, size_t pos)
{
	bitmap_t *bm = (bitmap_t*) hdl;
	unsigned long *p = ((unsigned long *)bm->bits) + BIT_WORD(pos);
	return (*p & BIT_MASK(pos)) != 0;
}

void bitmap_copy(bitmap_hdl dst, const bitmap_hdl src, size_t nbits)
{
	bitmap_t *dstbm = (bitmap_t*) dst;
	const bitmap_t * const srcbm = (const bitmap_t*) src;
	unsigned int len = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
	memcpy(dstbm->bits, srcbm->bits, len);
}

void bitmap_resize(bitmap_hdl hdl, size_t nbits)
{
	bitmap_t *bm = (bitmap_t*) hdl;
	size_t oldnbytes = BITS_TO_LONGS(bm->nbits) * sizeof(unsigned long);
	size_t newnbytes = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
	if (nbits != bm->nbits) {
		unsigned long *tmpbits;
		tmpbits = (unsigned long*) malloc(newnbytes);
		size_t minbytes = nbits < bm->nbits ? newnbytes : oldnbytes;
		memcpy(tmpbits, bm->bits, minbytes);
		free(bm->bits);
		bm->bits = tmpbits;
	}
}
