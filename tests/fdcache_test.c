#include <stdlib.h>
#include <time.h>
#include <CUnit/Basic.h>
#include <jemalloc/jemalloc.h>

#include "../fdcache.h"

/* test helpers */

void test_fdcache_creation_deletion()
{
}


int init_fdcache_test_suite(void) {
	/* init PRNG */
	srand(time(NULL));
	return 0;
}

int clean_fdcache_test_suite(void) { return 0; }

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

	if ((NULL == CU_add_test(pSuite, "fdcache creation/deletion", test_fdcache_creation_deletion))) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();
	rc = (CU_get_number_of_failures() != 0) ? 1 : 0;
	CU_cleanup_registry();

	malloc_stats_print(NULL, NULL, NULL);

	return rc;
}
