#include <time.h>
#include <stdlib.h>
#include "test_helpers.h"
#include "../bitmap.h"


/* forward declarations */
void randomize_bits(bitmap_hdl hdl);

void test_bitmap_alloc_free()
{
	bitmap_hdl bm;
	size_t nbits;
	size_t tidx;

	typedef struct test_table_ { size_t nbits; } test_table;

	test_table tt[]= {
		{ .nbits = 15},
		{ .nbits = 16},
		{ .nbits = 17},
		{ .nbits = 1023},
		{ .nbits = 1024},
		{ .nbits = 1025},
		{ .nbits = 160 << 20}
	};

	/* special case, nbits is 0 */
	nbits = 0;
	bm = bitmap_alloc(nbits);
	CU_ASSERT_PTR_NULL_FATAL(bm);
	bitmap_free(bm);

	for (tidx = 0; tidx < sizeof(tt) / sizeof(tt[0]); ++tidx) {
		bm = bitmap_alloc(tt[tidx].nbits);
		CU_ASSERT_PTR_NOT_NULL_FATAL(bm);
		bitmap_free(bm);
	}
}

void test_bitmap_get_set()
{
	bitmap_hdl bm;
	size_t tidx, i;

	typedef struct test_table_ { size_t nbits; } test_table;

	test_table tt[]= {
		{ .nbits = 15},
		{ .nbits = 16},
		{ .nbits = 17},
		{ .nbits = 1023},
		{ .nbits = 1024},
		{ .nbits = 1025},
		{ .nbits = 1024*1024}
	};

	for (tidx = 0; tidx < sizeof(tt) / sizeof(tt[0]); ++tidx) {
		bm = bitmap_alloc(tt[tidx].nbits);
		for (i = 0; i < tt[tidx].nbits; ++i) {
			bitmap_reset(bm, i);
			CU_ASSERT_FALSE_FATAL(bitmap_get(bm, i));
			bitmap_set(bm, i);
			CU_ASSERT_TRUE_FATAL(bitmap_get(bm, i));
			bitmap_reset(bm, i);
			CU_ASSERT_FALSE_FATAL(bitmap_get(bm, i));
		}
		bitmap_free(bm);
	}
}

void test_bitmap_zero_fill()
{
	bitmap_hdl bm;
	size_t tidx;

	typedef struct test_table_ { size_t nbits; } test_table;

	test_table tt[]= {
		{ .nbits = 15},
		{ .nbits = 16},
		{ .nbits = 17},
		{ .nbits = 1023},
		{ .nbits = 1024},
		{ .nbits = 1025},
		{ .nbits = 1024*1024}
	};

	for (tidx = 0; tidx < sizeof(tt) / sizeof(tt[0]); ++tidx) {
		bm = bitmap_alloc(tt[tidx].nbits);
		bitmap_fill(bm);

		/* test the first, the middle and the last bit */
		CU_ASSERT_TRUE_FATAL(bitmap_get(bm, 0));
		CU_ASSERT_TRUE_FATAL(bitmap_get(bm, tt[tidx].nbits - 1));
		CU_ASSERT_TRUE_FATAL(bitmap_get(bm, tt[tidx].nbits / 2));

		bitmap_zero(bm);

		/* test the first, the middle and the last bit */
		CU_ASSERT_FALSE_FATAL(bitmap_get(bm, 0));
		CU_ASSERT_FALSE_FATAL(bitmap_get(bm, tt[tidx].nbits - 1));
		CU_ASSERT_FALSE_FATAL(bitmap_get(bm, tt[tidx].nbits / 2));

		bitmap_fill(bm);

		/* test the first, the middle and the last bit */
		CU_ASSERT_TRUE_FATAL(bitmap_get(bm, 0));
		CU_ASSERT_TRUE_FATAL(bitmap_get(bm, tt[tidx].nbits - 1));
		CU_ASSERT_TRUE_FATAL(bitmap_get(bm, tt[tidx].nbits / 2));

		bitmap_free(bm);
	}
}

void test_bitmap_realloc()
{
	bitmap_hdl bm, tmp;
	size_t tidx, i;

	typedef struct test_table_ { size_t oldnbits, newnbits; } test_table;

	test_table tt[]= {
		{ .oldnbits = 15, .newnbits = 15},
		{ .oldnbits = 1, .newnbits = 15},
		{ .oldnbits = 15, .newnbits = 1},
		{ .oldnbits = 32, .newnbits = 128},
		{ .oldnbits = 128, .newnbits = 1},
		{ .oldnbits = 1, .newnbits = 1024},
		{ .oldnbits = 1024*32, .newnbits = 64},
	};

	for (tidx = 0; tidx < sizeof(tt) / sizeof(tt[0]); ++tidx) {
		bm = bitmap_alloc(tt[tidx].oldnbits);

		/* randomize bitmap */
		randomize_bits(bm);

		/* make a copy so we can compare valid indices after resizing */
		tmp = bitmap_alloc(tt[tidx].oldnbits);
		bitmap_copy(tmp, bm, tt[tidx].oldnbits);

		bitmap_realloc(bm, tt[tidx].newnbits);

		size_t minbits = min(tt[tidx].oldnbits, tt[tidx].newnbits);
		for (i = 0; i < minbits; ++i)
			CU_ASSERT_EQUAL(bitmap_get(bm, i), bitmap_get(tmp, i));

		bitmap_free(bm);
		bitmap_free(tmp);
	}
}

void test_bitmap_copy()
{
	bitmap_hdl srcbm, dstbm;
	size_t tidx, i;

	typedef struct test_table_ {
		size_t srcnbits;	/* number of bits in source bitmap */
		size_t dstnbits;	/* number of bits in dest bitmap */
		size_t nbits;		/* number of bits to copyn */
	} test_table;

	test_table tt[]= {
		{ .srcnbits = 1, .dstnbits = 2, .nbits = 1},
		{ .srcnbits = 65, .dstnbits = 66, .nbits = 2},
		{ .srcnbits = 65, .dstnbits = 66, .nbits = 65},
		{ .srcnbits = 128, .dstnbits = 12, .nbits = 10},
		{ .srcnbits = 12, .dstnbits = 128, .nbits = 10},
		{ .srcnbits = 1024, .dstnbits = 1024, .nbits = 1024},
		{ .srcnbits = 1020, .dstnbits = 1024, .nbits = 1020},
		{ .srcnbits = 1024, .dstnbits = 1020, .nbits = 1020},
		{ .srcnbits = 1024, .dstnbits = 1020, .nbits = 512},
		{ .srcnbits = 1024, .dstnbits = 1020, .nbits = 513},
		{ .srcnbits = 1024, .dstnbits = 1020, .nbits = 514},
	};

	for (tidx = 0; tidx < sizeof(tt) / sizeof(tt[0]); ++tidx) {

		/* create 2 bitmaps of possibily different size */
		srcbm = bitmap_alloc(tt[tidx].srcnbits);
		dstbm = bitmap_alloc(tt[tidx].dstnbits);

		/* randomize both bitmaps*/
		randomize_bits(srcbm);
		randomize_bits(dstbm);

		/* copy a range of bits from one bitmap to another */
		bitmap_copy(dstbm, srcbm, tt[tidx].nbits);

		/* check all bits in copied range are equal */
		for (i = 0; i < tt[tidx].nbits; ++i)
			CU_ASSERT_EQUAL(bitmap_get(srcbm, i), bitmap_get(dstbm, i));

		bitmap_free(srcbm);
		bitmap_free(dstbm);
	}
}

void test_bitmap_set_range()
{
	bitmap_hdl bm;
	size_t tidx;

	typedef struct test_table_ {
		size_t nbits;	/* bitmap length*/
		size_t pos;	/* range start */
		int len;	/* range length */
	} test_table;

	test_table tt[]= {
		{ .nbits = 1, .pos = 0, .len = 1},
		{ .nbits = 2, .pos = 0, .len = 1},
		{ .nbits = 2, .pos = 1, .len = 1},
		{ .nbits = 2, .pos = 0, .len = 2},
		{ .nbits = 4, .pos = 0, .len = 1},
		{ .nbits = 4, .pos = 1, .len = 2},
		{ .nbits = 4, .pos = 3, .len = 1},
		{ .nbits = 127, .pos = 0, .len = 127},
		{ .nbits = 127, .pos = 126, .len = 1},
		{ .nbits = 127, .pos = 60, .len = 4},
		{ .nbits = 127, .pos = 60, .len = 10},
		{ .nbits = 127, .pos = 64, .len = 1},
		{ .nbits = 127, .pos = 64, .len = 64},
		{ .nbits = 1024, .pos = 0, .len = 1024},
		{ .nbits = 1024, .pos = 1, .len = 1023},
		{ .nbits = 1024, .pos = 500, .len = 500},
		{ .nbits = 1024, .pos = 1000, .len = 1},
	};

	for (tidx = 0; tidx < sizeof(tt) / sizeof(tt[0]); ++tidx) {
		test_table *t = &tt[tidx];
		size_t i;

		bm = bitmap_alloc(t->nbits);
		bitmap_zero(bm);
		bitmap_set_range(bm, t->pos, t->len);

		for (i = 0; i < t->pos; ++i)
			CU_ASSERT_EQUAL_FATAL(false, bitmap_get(bm, i));

		for (i = t->pos; i < t->pos + t->len; ++i)
			CU_ASSERT_EQUAL_FATAL(true, bitmap_get(bm, i));

		for (i = t->pos + t->len; i < t->nbits; ++i)
			CU_ASSERT_EQUAL_FATAL(false, bitmap_get(bm, i));

		bitmap_free(bm);
	}
}

void test_bitmap_get_range()
{
	bitmap_hdl bm;
	size_t tidx;

	typedef struct test_table_ {
		size_t nbits;	/* bitmap length*/
		size_t pos;	/* range start */
		int len;	/* range length */
	} test_table;

	test_table tt[]= {
		{ .nbits = 63},
		{ .nbits = 64},
		{ .nbits = 65},
		{ .nbits = 127},
		{ .nbits = 128},
		{ .nbits = 129},
		{ .nbits = 1023},
		{ .nbits = 1024},
		{ .nbits = 1025},
	};

	for (tidx = 0; tidx < sizeof(tt) / sizeof(tt[0]); ++tidx) {
		test_table *t = &tt[tidx];

		bm = bitmap_alloc(t->nbits);
		bitmap_fill(bm);

		CU_ASSERT_EQUAL_FATAL(true, bitmap_get_range(bm, 0, t->nbits - 1));

		bitmap_reset(bm, 0);
		CU_ASSERT_EQUAL_FATAL(false, bitmap_get_range(bm, 0, t->nbits - 1));
		CU_ASSERT_EQUAL_FATAL(true, bitmap_get_range(bm, 1, t->nbits - 2));

		bitmap_reset(bm, t->nbits - 1);
		CU_ASSERT_EQUAL_FATAL(false, bitmap_get_range(bm, 0, t->nbits - 1));
		CU_ASSERT_EQUAL_FATAL(true, bitmap_get_range(bm, 1, t->nbits - 3));

		bitmap_free(bm);
	}
}

void test_bitmap_reset_range()
{
	bitmap_hdl bm;
	size_t tidx;

	typedef struct test_table_ {
		size_t nbits;	/* bitmap length*/
		size_t pos;	/* range start */
		int len;	/* range length */
	} test_table;

	test_table tt[]= {
		{ .nbits = 1, .pos = 0, .len = 1},
		{ .nbits = 2, .pos = 0, .len = 1},
		{ .nbits = 2, .pos = 1, .len = 1},
		{ .nbits = 2, .pos = 0, .len = 2},
		{ .nbits = 4, .pos = 0, .len = 1},
		{ .nbits = 4, .pos = 1, .len = 2},
		{ .nbits = 4, .pos = 3, .len = 1},
		{ .nbits = 127, .pos = 0, .len = 127},
		{ .nbits = 127, .pos = 126, .len = 1},
		{ .nbits = 127, .pos = 60, .len = 4},
		{ .nbits = 127, .pos = 60, .len = 10},
		{ .nbits = 127, .pos = 64, .len = 1},
		{ .nbits = 127, .pos = 64, .len = 64},
		{ .nbits = 1024, .pos = 0, .len = 1024},
		{ .nbits = 1024, .pos = 1, .len = 1023},
		{ .nbits = 1024, .pos = 500, .len = 500},
		{ .nbits = 1024, .pos = 1000, .len = 1},
	};

	for (tidx = 0; tidx < sizeof(tt) / sizeof(tt[0]); ++tidx) {
		test_table *t = &tt[tidx];
		size_t i;

		bm = bitmap_alloc(t->nbits);
		bitmap_fill(bm);
		bitmap_reset_range(bm, t->pos, t->len);

		for (i = 0; i < t->pos; ++i)
			CU_ASSERT_EQUAL_FATAL(true, bitmap_get(bm, i));

		for (i = t->pos; i < t->pos + t->len; ++i)
			CU_ASSERT_EQUAL_FATAL(false, bitmap_get(bm, i));

		for (i = t->pos + t->len; i < t->nbits; ++i)
			CU_ASSERT_EQUAL_FATAL(true, bitmap_get(bm, i));

		bitmap_free(bm);
	}
}

void test_bitmap_count_setbits()
{
	size_t tidx;
	size_t last_nbits = 0;
	bitmap_hdl bm = 0;

	typedef struct test_table_ {
		size_t setpos;		/* bit to set */
		size_t want_setbits;	/* expected number of set bits */
		size_t nbits;		/* bitmap length (if different than last  */
					/* test, bitmap is resized and zero'ed)*/
	} test_table;

	test_table tt[]= {
		{ .nbits = 127,  .setpos = 0,		.want_setbits = 1 },
		{ .nbits = 127,  .setpos = 126,		.want_setbits = 2 },
		{ .nbits = 1024, .setpos = 0,		.want_setbits = 1 },
		{ .nbits = 1024, .setpos = 1,		.want_setbits = 2 },
		{ .nbits = 1024, .setpos = 512,		.want_setbits = 3 },
		{ .nbits = 1023, .setpos = 0,		.want_setbits = 1 },
		{ .nbits = 1023, .setpos = 1022,	.want_setbits = 2 },
		{ .nbits = 1023, .setpos = 512,		.want_setbits = 3 },
		{ .nbits = 1025, .setpos = 0,		.want_setbits = 1 },
		{ .nbits = 1025, .setpos = 1022,	.want_setbits = 2 },
		{ .nbits = 1025, .setpos = 1023,	.want_setbits = 3 },
		{ .nbits = 1025, .setpos = 1024,	.want_setbits = 4 },
		{ .nbits = 1025, .setpos = 512,		.want_setbits = 5 },
		{ .nbits = 1025, .setpos = 1,		.want_setbits = 6 },
		{ .nbits = 1025, .setpos = 2,		.want_setbits = 7 },
		{ .nbits = 1025, .setpos = 3,		.want_setbits = 8 },
		{ .nbits = 1025, .setpos = 4,		.want_setbits = 9 },
		{ .nbits = 1025, .setpos = 5,		.want_setbits = 10 },
		{ .nbits = 1025, .setpos = 65,		.want_setbits = 11 },
		{ .nbits = 1025, .setpos = 64,		.want_setbits = 12 },
		{ .nbits = 1025, .setpos = 63,		.want_setbits = 13 },
		{ .nbits = 1025, .setpos = 1020,	.want_setbits = 14 },
		{ .nbits = 1025, .setpos = 1019,	.want_setbits = 15 },
	};

	for (tidx = 0; tidx < sizeof(tt) / sizeof(tt[0]); ++tidx) {
		test_table *t = &tt[tidx];

		if (last_nbits != t->nbits) {
			if (!last_nbits)
				bm = bitmap_alloc(t->nbits);
			else
				bitmap_realloc(bm, t->nbits);
			bitmap_zero(bm);
			last_nbits = t->nbits;
		}

		bitmap_set(bm, t->setpos);
		CU_ASSERT_EQUAL_FATAL(t->want_setbits, bitmap_count_setbits(bm));
	}

	bitmap_free(bm);
}

int init_bitmap_test_suite(void) {
	/* init PRNG */
	srand(time(NULL));
	return 0;
}

int clean_bitmap_test_suite(void) { return 0; }

void randomize_bits(bitmap_hdl hdl)
{
	size_t i = 0;
	size_t b = 0;
	size_t nbits = bitmap_length(hdl);
	while (i < nbits)
	{
		int r = rand();
		for (b = 0; (i < nbits) && (b < sizeof(int) * 8); ++b, ++i) {

			if ((1UL << ((b) % 32)) & r)
				bitmap_set(hdl, i);
			else
				bitmap_reset(hdl, i);
		}
	}
}

int main()
{
	int rc = EXIT_FAILURE;
	CU_pSuite pSuite = NULL;

	if (CUE_SUCCESS != CU_initialize_registry())
		return CU_get_error();

	pSuite = CU_add_suite("bitmap_suite", init_bitmap_test_suite, clean_bitmap_test_suite);
	if (NULL == pSuite) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	if ((NULL == CU_add_test(pSuite, "bitmap alloc/free", test_bitmap_alloc_free)) ||
	    (NULL == CU_add_test(pSuite, "bitmap get/set", test_bitmap_get_set)) ||
	    (NULL == CU_add_test(pSuite, "bitmap realloc", test_bitmap_realloc)) ||
	    (NULL == CU_add_test(pSuite, "bitmap copy", test_bitmap_copy)) ||
	    (NULL == CU_add_test(pSuite, "bitmap range set", test_bitmap_set_range)) ||
	    (NULL == CU_add_test(pSuite, "bitmap range get", test_bitmap_get_range)) ||
	    (NULL == CU_add_test(pSuite, "bitmap range reset", test_bitmap_reset_range)) ||
	    (NULL == CU_add_test(pSuite, "bitmap zero/fill", test_bitmap_zero_fill)) ||
	    (NULL == CU_add_test(pSuite, "bitmap count set bits", test_bitmap_count_setbits))) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();
	rc = (CU_get_number_of_failures() != 0) ? 1 : 0;
	CU_cleanup_registry();
	return rc;
}
