#include <stdlib.h>
#include <stdio.h>
#include <jemalloc/jemalloc.h>
#include "test_helpers.h"


size_t __report_allocated(size_t *epoch)
{
	size_t sz, allocated;

	/* update epoch */
	sz = sizeof(epoch);
	mallctl("epoch", &epoch, &sz, &epoch, sz);

	/* retrieve allocated memory */
	sz = sizeof(size_t);
	mallctl("stats.allocated", &allocated, &sz, NULL, 0);
	return allocated;
}

char * __sprint_buffer(char *dst, const char *buf, size_t len)
{
	size_t i = -1;
	while(++i < len)
		sprintf(dst + i*5, "0x%02x ", (unsigned char) (buf[i]));
	return dst;
}

size_t min(size_t a, size_t b)
{
	return a < b ? a : b;
}
