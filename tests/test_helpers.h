#ifndef FDCACHE_TEST_HELPERS_H
#define FDCACHE_TEST_HELPERS_H

#include <CUnit/Basic.h>


char * __sprint_buffer(char * dst, const char *buf, size_t len);
size_t __report_allocated(size_t *epoch);
size_t min(size_t a, size_t b);

#define __PRINT_BUF_MAXLEN 128

#define __ASSERT_EQUAL_BUFFER(got, want, len, print_on_error, fatal) ({ \
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

#define CU_ASSERT_EQUAL_BUFFER(got, want, len)\
	__ASSERT_EQUAL_BUFFER(got, want, len, (len<=__PRINT_BUF_MAXLEN),CU_FALSE)

#define CU_ASSERT_EQUAL_BUFFER_FATAL(got, want, len)\
	__ASSERT_EQUAL_BUFFER(got, want, len, (len<=__PRINT_BUF_MAXLEN),CU_TRUE)

#define CU_ASSERT_RC_SUCCESS(__function, ...) ({\
	int __rc = __function(__VA_ARGS__);\
	CU_ASSERT_EQUAL_FATAL(__rc, 0); })

#define CU_ASSERT_RC_EQUAL(__rcwant, __function, ...) ({\
	int __rc = __function(__VA_ARGS__);\
	CU_ASSERT_EQUAL_FATAL(__rc, __rcwant); })

#define CU_LEAK_CHECK_BEGIN size_t epoch = 0; ssize_t __alloc_1 = (ssize_t) __report_allocated(&epoch);
#define CU_LEAK_CHECK_END do {\
	ssize_t __alloc_diff = (ssize_t) __report_allocated(&epoch) - __alloc_1;\
	if (__alloc_diff <= 0) {\
		CU_PASS("allocation differential is null");\
	} else {\
		char failmsg[256];\
		sprintf(failmsg, "positive allocation differential %lub", __alloc_diff);\
		CU_assertImplementation(CU_FALSE, __LINE__, failmsg, __FILE__, __func__, CU_FALSE);\
	}} while(0)

#endif
