#include <jemalloc/jemalloc.h>
#include <assert.h>
#include "fdcache.h"

int main()
{
	kvsns_ino_t ino1 = 1;
	size_t multipart_limit = 5 << 20;	// 5 Megabytes
	size_t ram_fs_limit = 1024 << 20;	// 1 Gigabytes

	fdc_init(ram_fs_limit);
	fd_cache_t ice1 = fdc_get_or_create(ino1, multipart_limit);

	fdc_write(ice1, "data", 2, 10);
	char buf[64];
	fdc_read(ice1, buf, 2, 10);
	buf[2] = '\0';
	printf("read: %s\n", buf);

	assert(strncmp(buf, "da", 2) == 0);

	malloc_stats_print(NULL, NULL, NULL);

	fdc_write(ice1, "data", 2, 2 << 20);

	malloc_stats_print(NULL, NULL, NULL);

	return 0;
}
