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

/**
 * @brief fdc_get_or_create create a new cache entry associated with the client
 *                          id `ino` or retrieve the entry if it already exists.
 * @param ino [IN] client inode number
 * @param block_size [IN] size of block, should be set to the size of the
 *                        buffers used to write data to the fd cache. (prefered
 *                        write size).
 * @param blocks_per_cluster [IN] number of blocks per cluster, should be set so
 *                        that the size of one cluster corresponds to the size
 *                        of buffers used to read data from the fd cache.
 *                        (prefered read size).
 * @param fd [OUT] on success, the value pointed to by fd will be set to the
 *                        opaque fd_cache pointer, used afterwards to read/write
 *                        to the fd cache. On error, its value is undefined.
 * @return 0 on success or negative errno values. Possible error codes:
 *	- -EINVAL for invalid arguments
 *	- -ENFILE if the maximum number of cache entries has been reached.
 */
int fdc_get_or_create(kvsns_ino_t ino, size_t block_size, size_t blocks_per_cluster, fd_cache_t *fd);

// will return 0 or positive or negative error codes
// full_cluster (should prbably a list of the full clusters after this write)
ssize_t fdc_write(fd_cache_t fd, const void *buf, size_t count, off_t offset, ssize_t *full_cluster);

// will return 0 or positive or negative error codes
ssize_t fdc_read(fd_cache_t fd, void *buf, size_t count, off_t offset);

#endif
