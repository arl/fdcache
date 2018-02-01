#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "test_helpers.h"
#include "../fdcache.h"
#include "../fdcache_internal.h"


/* fdcache test suite */

void test_fdcache_creation_deletion()
{
	CU_LEAK_CHECK_BEGIN;

	cache_ino_t ino1 = 1;
	size_t multipart_limit = 5 << 20;	// 5 Megabytes
	size_t ram_fs_limit = 1024 << 20;	// 1 Gigabytes
	fd_cache_t ice1;

	fdc_init(ram_fs_limit);
	CU_ASSERT_RC_SUCCESS(fdc_get_or_create, ino1, multipart_limit, 5, &ice1);
	fdc_deinit();

	CU_LEAK_CHECK_END;
}

void test_fdcache_read_write_ram()
{
	CU_LEAK_CHECK_BEGIN;

	cache_ino_t ino1 = 1;
	size_t ram_fs_limit = 1024 << 20;	/* 1024 MB */
	ssize_t full_cluster;
	size_t tidx;
	fd_cache_t ice1;
	const char refbuf[] = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x10\x11\x12\x13\x14\x15";

	typedef struct test_table_ {
		size_t block_size;		/* block size */
		size_t blocks_per_cluster;	/* number of blocks in one region */
		size_t count;
		size_t offset;
		ssize_t nwritten;	/* number of bytes written or negative error code */
	} test_table;

	test_table tt[]= {
		{ .block_size = 1, .blocks_per_cluster = 1, .count = 1, .offset = 0, .nwritten = 1},
		{ .block_size = 1, .blocks_per_cluster = 2, .count = 2, .offset = 0, .nwritten = 2},
		{ .block_size = 1, .blocks_per_cluster = 3, .count = 3, .offset = 0, .nwritten = 3},
		{ .block_size = 1, .blocks_per_cluster = 4, .count = 4, .offset = 0, .nwritten = 4},
		{ .block_size = 2, .blocks_per_cluster = 1, .count = 1, .offset = 0, .nwritten = -ESPIPE},
		{ .block_size = 2, .blocks_per_cluster = 2, .count = 0, .offset = 1, .nwritten = -ESPIPE},
		{ .block_size = 2, .blocks_per_cluster = 1, .count = 3, .offset = 0, .nwritten = -ESPIPE},
		{ .block_size = 2, .blocks_per_cluster = 2, .count = 0, .offset = 3, .nwritten = -ESPIPE},
		{ .block_size = 2, .blocks_per_cluster = 3, .count = 0, .offset = 6, .nwritten = 0},
		{ .block_size = 2, .blocks_per_cluster = 3, .count = 6, .offset = 12, .nwritten = 6},
		{ .block_size = 2, .blocks_per_cluster = 3, .count = 12, .offset = 12, .nwritten = 12},
		{ .block_size = 2, .blocks_per_cluster = 4, .count = 8, .offset = 0, .nwritten = 8},
		{ .block_size = 3, .blocks_per_cluster = 1, .count = 3, .offset = 0, .nwritten = 3},
		{ .block_size = 4, .blocks_per_cluster = 2, .count = 8, .offset = 0, .nwritten = 8},
		{ .block_size = 3, .blocks_per_cluster = 3, .count = 9, .offset = 0, .nwritten = 9},
		{ .block_size = 3, .blocks_per_cluster = 4, .count = 12, .offset = 0, .nwritten = 12},
		{ .block_size = 4, .blocks_per_cluster = 1, .count = 4, .offset = 0, .nwritten = 4},
		{ .block_size = 4, .blocks_per_cluster = 2, .count = 8, .offset = 0, .nwritten = 8},
		{ .block_size = 4, .blocks_per_cluster = 3, .count = 12, .offset = 0, .nwritten = 12},
		{ .block_size = 4, .blocks_per_cluster = 4, .count = 16, .offset = 0, .nwritten = 16},
		{ .block_size = 2, .blocks_per_cluster = 4, .count = 8, .offset = 0, .nwritten = 8},
		{ .block_size = 3, .blocks_per_cluster = 1, .count = 3, .offset = 0, .nwritten = 3},
		{ .block_size = 4, .blocks_per_cluster = 2, .count = 8, .offset = 0, .nwritten = 8},
		{ .block_size = 3, .blocks_per_cluster = 3, .count = 9, .offset = 0, .nwritten = 9},
		{ .block_size = 3, .blocks_per_cluster = 4, .count = 12, .offset = 0, .nwritten = 12},
		{ .block_size = 4, .blocks_per_cluster = 1, .count = 4, .offset = 0, .nwritten = 4},
		{ .block_size = 4, .blocks_per_cluster = 2, .count = 8, .offset = 0, .nwritten = 8},
		{ .block_size = 4, .blocks_per_cluster = 3, .count = 12, .offset = 0, .nwritten = 12},
		{ .block_size = 4, .blocks_per_cluster = 4, .count = 16, .offset = 0, .nwritten = 16},
	};

	for (tidx = 0; tidx < sizeof(tt) / sizeof(tt[0]); ++tidx) {
		test_table *t = &tt[tidx];

		printf("%s block_size=%lu blocks_per_cluster=%lu\n",
		       __func__, t->block_size, t->blocks_per_cluster);

		fdc_init(ram_fs_limit);

		CU_ASSERT_RC_SUCCESS(fdc_get_or_create, ino1, t->block_size, t->blocks_per_cluster, &ice1);
		CU_ASSERT_EQUAL_FATAL(t->nwritten, fdc_write(ice1, refbuf, t->count, t->offset, &full_cluster));
		if (t->nwritten >= 0) {
			char readbuf[16];
			memset(readbuf, 0xff, sizeof(readbuf) / sizeof(char));
			CU_ASSERT_EQUAL_FATAL(t->count, fdc_read(ice1, readbuf, t->count, t->offset));
			CU_ASSERT_EQUAL_BUFFER(readbuf, refbuf, t->count);
		}

		fdc_deinit();
	}

	CU_LEAK_CHECK_END;
}

void test_fdcache_get_or_create_return_codes()
{
	CU_LEAK_CHECK_BEGIN;

	const size_t max_cache_entries = 20;
	size_t ram_fs_limit = 1024 << 20;
	size_t i;
	fd_cache_t ice1;

	fdc_init(ram_fs_limit);

	/* invalid block size */
	CU_ASSERT_RC_EQUAL(-EINVAL, fdc_get_or_create, 0, 0, 1, &ice1);
	/* invalid cluster per blocks */
	CU_ASSERT_RC_EQUAL(-EINVAL, fdc_get_or_create, 0, 1, 0, &ice1);
	/* both block size and cluster per blocks invalid */
	CU_ASSERT_RC_EQUAL(-EINVAL, fdc_get_or_create, 0, 0, 0, &ice1);

	/* create the maximum number of cache entries */
	for (i = 0; i < max_cache_entries; ++i)
		CU_ASSERT_RC_SUCCESS(fdc_get_or_create, i, 1, 1, &ice1);

	/* try to create another one, should fail */
	CU_ASSERT_RC_EQUAL(-ENFILE, fdc_get_or_create, i, 1, 1, &ice1);

	fdc_deinit();

	CU_LEAK_CHECK_END;
}

void test_fdcache_read_return_codes()
{
	CU_LEAK_CHECK_BEGIN;

	size_t ram_fs_limit = 1024 << 20;	/* 1024 MB */
	ssize_t full_cluster;
	char buf[16];
	const char refbuf[] = "\x00\x01\x02\x03\x04\x05\x06\x07";
	fd_cache_t ice1;

	fdc_init(ram_fs_limit);

	CU_ASSERT_RC_SUCCESS(fdc_get_or_create, 0, 2, 2, &ice1);

	/* negative offset */
	CU_ASSERT_RC_EQUAL(-EINVAL, fdc_read, ice1, buf, 16, -1);

	/* write 2 bytes at offset 0 */
	CU_ASSERT_EQUAL(2, fdc_write(ice1, refbuf, 2, 0, &full_cluster));
	/* write 2 bytes at offset 14, making the entry 16 bytes long */
	CU_ASSERT_EQUAL(2, fdc_write(ice1, refbuf, 2, 14, &full_cluster));

	/* reading past buffer end */
	CU_ASSERT_RC_EQUAL(-EOVERFLOW, fdc_read, ice1, buf, 1, 16);
	CU_ASSERT_RC_EQUAL(-EOVERFLOW, fdc_read, ice1, buf, 17, 1);

	/* read allocated clusters */
	CU_ASSERT_RC_EQUAL(1, fdc_read, ice1, buf, 1, 0);
	CU_ASSERT_RC_EQUAL(2, fdc_read, ice1, buf, 2, 0);
	CU_ASSERT_RC_EQUAL(3, fdc_read, ice1, buf, 3, 1);

	CU_ASSERT_RC_EQUAL(1, fdc_read, ice1, buf, 1, 14);
	CU_ASSERT_RC_EQUAL(2, fdc_read, ice1, buf, 2, 13);
	CU_ASSERT_RC_EQUAL(3, fdc_read, ice1, buf, 3, 12);

	/* trying to read unallocated clusters */
	CU_ASSERT_RC_EQUAL(-EFAULT, fdc_read, ice1, buf, 1, 4);
	CU_ASSERT_RC_EQUAL(-EFAULT, fdc_read, ice1, buf, 1, 8);

	fdc_deinit();

	CU_LEAK_CHECK_END;
}

void test_fdcache_entry_size_mem()
{
	CU_LEAK_CHECK_BEGIN;

	size_t ram_fs_limit = 1024 << 20;	/* 1024 MB */
	ssize_t full_cluster;
	const char refbuf[] = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x10\x11\x12\x13\x14\x15";
	const char longbuf[1024];
	size_t nbytes;
	fd_cache_t ice1;
	cache_ino_t ino = 0;

	fdc_init(ram_fs_limit);

	/* trying to retrieve size of an unknown entry (ino = 0)*/
	ino = 0;
	CU_ASSERT_RC_EQUAL(-EFAULT, fdc_entry_size, ino, &nbytes);

	CU_ASSERT_RC_SUCCESS(fdc_get_or_create, ino, 2, 2, &ice1);

	/* write 2 bytes at offset 0 */
	CU_ASSERT_EQUAL(2, fdc_write(ice1, refbuf, 2, 0, &full_cluster));
	/* check size = 2, mem = 4 */
	CU_ASSERT_RC_SUCCESS(fdc_entry_size, ino, &nbytes);
	CU_ASSERT_EQUAL(2, nbytes);
	CU_ASSERT_RC_SUCCESS(fdc_entry_mem, ino, &nbytes);
	CU_ASSERT_EQUAL(4, nbytes);

	/* write 2 bytes at offset 2 */
	CU_ASSERT_EQUAL(2, fdc_write(ice1, refbuf + 2, 2, 2, &full_cluster));
	/* check size = 4, mem = 4 */
	CU_ASSERT_RC_SUCCESS(fdc_entry_size, ino, &nbytes);
	CU_ASSERT_EQUAL(4, nbytes);
	CU_ASSERT_RC_SUCCESS(fdc_entry_mem, ino, &nbytes);
	CU_ASSERT_EQUAL(4, nbytes);

	/* write 2 bytes at offset 10 */
	CU_ASSERT_EQUAL(2, fdc_write(ice1, refbuf + 10, 2, 10, &full_cluster));
	/* check size = 12, mem = 8 */
	CU_ASSERT_RC_SUCCESS(fdc_entry_size, ino, &nbytes);
	CU_ASSERT_EQUAL(12, nbytes);
	CU_ASSERT_RC_SUCCESS(fdc_entry_mem, ino, &nbytes);
	CU_ASSERT_EQUAL(8, nbytes);

	/* check written buffer correctness */
	char readbuf[16];
	memset(readbuf, 0xff, sizeof(readbuf) / sizeof(char));
	fdc_read(ice1, readbuf, 4, 0);
	fdc_read(ice1, readbuf +10, 2, 10);
	CU_ASSERT_EQUAL_BUFFER(readbuf, "\x00\x01\x02\x03\xff\xff\xff\xff\xff\xff\x10\x11\xff\xff\xff\xff", 16);

	/* trying to retrieve size of an unknown entry (ino = 1) */
	ino = 1;
	CU_ASSERT_RC_EQUAL(-EFAULT, fdc_entry_size, ino, &nbytes);

	CU_ASSERT_RC_SUCCESS(fdc_get_or_create, ino, 512, 2, &ice1);

	/* check size is empty */
	CU_ASSERT_RC_SUCCESS(fdc_entry_size, ino, &nbytes);
	CU_ASSERT_EQUAL(0, nbytes);
	CU_ASSERT_RC_SUCCESS(fdc_entry_mem, ino, &nbytes);
	CU_ASSERT_EQUAL(0, nbytes);

	/* write 1024 bytes at offset 1024 (1 cluster) */
	CU_ASSERT_EQUAL(1024, fdc_write(ice1, longbuf, 1024, 1024, &full_cluster));
	/* check size = 2048, mem = 1024 */
	CU_ASSERT_RC_SUCCESS(fdc_entry_size, ino, &nbytes);
	CU_ASSERT_EQUAL(2048, nbytes);
	CU_ASSERT_RC_SUCCESS(fdc_entry_mem, ino, &nbytes);
	CU_ASSERT_EQUAL(1024, nbytes);

	/* write 512 bytes at offset 512 */
	CU_ASSERT_EQUAL(512, fdc_write(ice1, refbuf, 512, 512, &full_cluster));
	/* check size = 2048, mem = 2048 (2 clusters) */
	CU_ASSERT_RC_SUCCESS(fdc_entry_size, ino, &nbytes);
	CU_ASSERT_EQUAL(2048, nbytes);
	CU_ASSERT_RC_SUCCESS(fdc_entry_mem, ino, &nbytes);
	CU_ASSERT_EQUAL(2048, nbytes);

	/* write 4096 bytes at offset 1MB */
	CU_ASSERT_EQUAL(4096, fdc_write(ice1, refbuf, 4096, 1024 * 1024, &full_cluster));
	/* check size = 4096+1MB, mem = (2+4) * 1024 (6 clusters) */
	CU_ASSERT_RC_SUCCESS(fdc_entry_size, ino, &nbytes);
	CU_ASSERT_EQUAL(4096 + 1024 * 1024, nbytes);
	CU_ASSERT_RC_SUCCESS(fdc_entry_mem, ino, &nbytes);
	CU_ASSERT_EQUAL(6 * 1024, nbytes);

	fdc_deinit();

	CU_LEAK_CHECK_END;
}

int init_fdcache_test_suite()
{
	/* init PRNG */
	srand(time(NULL));
	return 0;
}

int clean_fdcache_test_suite()
{
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
	    (NULL == CU_add_test(pSuite, "fdcache read/write RAM", test_fdcache_read_write_ram)) ||
	    (NULL == CU_add_test(pSuite, "fdcache get_or_create return codes", test_fdcache_get_or_create_return_codes)) ||
	    (NULL == CU_add_test(pSuite, "fdcache read return codes", test_fdcache_read_return_codes)) ||
	    (NULL == CU_add_test(pSuite, "fdcache entry size/mem", test_fdcache_entry_size_mem))) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();
	rc = (CU_get_number_of_failures() != 0) ? 1 : 0;
	CU_cleanup_registry();

	return rc;
}
