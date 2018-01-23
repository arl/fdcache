#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <CUnit/Basic.h>
#include <jemalloc/jemalloc.h>

#include "../fdcache.h"

/* test helpers */

char * __sprint_buffer(char * dst, const char *buf, size_t len)
{
	size_t i = -1;
	while(++i < len)
		sprintf(dst + i*5, "0x%02x ", (unsigned char)(buf[i]));
	return dst;
}

#define __PRINT_BUF_MAXLEN 128

#define __ASSERT_EQUAL_BUFFER(got,want,len,print_on_error,fatal) ({ \
	if (memcmp(got, want, len)) { \
		char failmsg[__PRINT_BUF_MAXLEN * 16]; \
		char bufdisp[__PRINT_BUF_MAXLEN * 6]; \
		sprintf(failmsg, "buffers are not equal,\n"); \
		if (print_on_error) { \
			strcat(failmsg, "want = "); \
			__sprint_buffer(bufdisp, want, len); \
			strcat(failmsg, bufdisp); \
			strcat(failmsg, "\ngot  = "); \
			__sprint_buffer(bufdisp, got, len); \
			strcat(failmsg, bufdisp); \
		} else { \
			sprintf(failmsg, "buffers are not equal"); \
		} \
		{ CU_assertImplementation(CU_FALSE, __LINE__, failmsg, __FILE__, __func__, fatal); } \
	} else { \
		CU_PASS("buffers are equal"); \
	} \
})

#define CU_ASSERT_EQUAL_BUFFER(got,want,len)\
	__ASSERT_EQUAL_BUFFER(got,want,len,(len<=__PRINT_BUF_MAXLEN),CU_FALSE)

#define CU_ASSERT_EQUAL_BUFFER_FATAL(got,want,len)\
	__ASSERT_EQUAL_BUFFER(got,want,len,(len<=__PRINT_BUF_MAXLEN),CU_TRUE)

/* fdcache test suite */

void test_fdcache_creation_deletion()
{
	kvsns_ino_t ino1 = 1;
	size_t multipart_limit = 5 << 20;	// 5 Megabytes
	size_t ram_fs_limit = 1024 << 20;	// 1 Gigabytes

	fdc_init(ram_fs_limit);
	fd_cache_t ice1 = fdc_get_or_create(ino1, multipart_limit, 5);

	fdc_deinit();
}

void test_fdcache_read_write_ram()
{
	kvsns_ino_t ino1 = 1;
	size_t ram_fs_limit = 1024 << 20;	/* 1024 MB */
	ssize_t full_cluster;
	size_t tidx;

	typedef struct test_table_ {
		size_t block_size;		/* block size */
		size_t blocks_per_cluster;	/* number of blocks in one region */
	} test_table;

	test_table tt[]= {
		{ .block_size = 1, .blocks_per_cluster = 1},
		{ .block_size = 1, .blocks_per_cluster = 2},
		{ .block_size = 1, .blocks_per_cluster = 3},
		{ .block_size = 1, .blocks_per_cluster = 4},
		{ .block_size = 2, .blocks_per_cluster = 1},
		{ .block_size = 2, .blocks_per_cluster = 2},
		{ .block_size = 2, .blocks_per_cluster = 3},
		{ .block_size = 2, .blocks_per_cluster = 4},
		{ .block_size = 3, .blocks_per_cluster = 1},
		{ .block_size = 4, .blocks_per_cluster = 2},
		{ .block_size = 3, .blocks_per_cluster = 3},
		{ .block_size = 3, .blocks_per_cluster = 4},
		{ .block_size = 4, .blocks_per_cluster = 1},
		{ .block_size = 4, .blocks_per_cluster = 2},
		{ .block_size = 4, .blocks_per_cluster = 3},
		{ .block_size = 4, .blocks_per_cluster = 4},
	};

	for (tidx = 0; tidx < sizeof(tt) / sizeof(tt[0]); ++tidx) {
		test_table *t = &tt[tidx];

		printf("%s block_size=%lu blocks_per_cluster=%lu\n",
		       __func__, t->block_size, t->blocks_per_cluster);

		fdc_init(ram_fs_limit);

		fd_cache_t ice1 = fdc_get_or_create(ino1, t->block_size, t->blocks_per_cluster);

		fdc_write(ice1, "\x00", 1, 0, &full_cluster);
		fdc_write(ice1, "\x01\x02\x03", 3, 1, &full_cluster);

		char got[4];

		memset(got, 0xff, 4);
		fdc_read(ice1, got, 4, 0);
		CU_ASSERT_EQUAL_BUFFER(got, "\x00\x01\x02\x03", 4);

		memset(got, 0xff, 4);
		fdc_read(ice1, got, 2, 1);
		CU_ASSERT_EQUAL_BUFFER(got, "\x01\x02", 2);

		memset(got, 0xff, 4);
		fdc_read(ice1, got, 3, 1);
		CU_ASSERT_EQUAL_BUFFER(got, "\x01\x02\x03", 1);

		memset(got, 0xff, 4);
		fdc_read(ice1, got, 2, 2);
		CU_ASSERT_EQUAL_BUFFER(got, "\x02\x03", 2);

		memset(got, 0xff, 4);
		fdc_read(ice1, got, 1, 3);
		CU_ASSERT_EQUAL_BUFFER(got, "\x03", 1);

		fdc_deinit();
	}
}

int init_fdcache_test_suite(void) {
	/* init PRNG */
	srand(time(NULL));
	return 0;
}

int clean_fdcache_test_suite(void) {
	/* malloc_stats_print(NULL, NULL, NULL); */
	return 0;
}

int main()
{
	int rc = EXIT_FAILURE;
	CU_pSuite pSuite = NULL;

	if (CUE_SUCCESS != CU_initialize_registry())
		return CU_get_error();

	pSuite = CU_add_suite("fdcache_suite", init_fdcache_test_suite, clean_fdcache_test_suite);
	if (NULL == pSuite) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	if ((NULL == CU_add_test(pSuite, "fdcache creation/deletion", test_fdcache_creation_deletion)) ||
	    (NULL == CU_add_test(pSuite, "fdcache read/write RAM", test_fdcache_read_write_ram))) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();
	rc = (CU_get_number_of_failures() != 0) ? 1 : 0;
	CU_cleanup_registry();

	return rc;
}
