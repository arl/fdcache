#ifndef FDCACHE_H
#define FDCACHE_H

#include <stdio.h>
#include <string.h>

typedef unsigned long long int kvsns_ino_t;

/* Opaque file descriptor cache pointer */
typedef void* fd_cache_t;

/* Initialize fdcache and set the RAM-filesystem limit (in bytes) */
void fdc_init(size_t ram_fs_limit);

/* Clean resources allocated by fdcache library */
void fdc_deinit();

fd_cache_t fdc_get_or_create(kvsns_ino_t ino, size_t block_size, size_t blocks_per_cluster);

// will return 0 or positive or negative error codes
// full_cluster (should prbably a list of the full clusters after this write)
ssize_t fdc_write(fd_cache_t fd, const void *buf, size_t count, off_t offset, ssize_t *full_cluster);

// will return 0 or positive or negative error codes
ssize_t fdc_read(fd_cache_t fd, void *buf, size_t count, off_t offset);

#endif
