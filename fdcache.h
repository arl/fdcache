#ifndef FDCACHE_H
#define FDCACHE_H

#include <stdio.h>
#include <string.h>

/**
 * @brief cache_ino_t type of the client cached inode.
 */
typedef unsigned long long int cache_ino_t;

/**
 * @brief fd_cache_t opaque file descriptor cache pointer
 */
typedef void* fd_cache_t;

/**
 * @brief fdc_init initialize fdcache library and set the RAM-filesystem limit.
 * @param ram_fs_limit [IN] maximum size (in bytes) of a cache entry in RAM, if
 *                          an entry grows over this size, it gets moved to the
 *                          filesystem.
 */
void fdc_init(size_t ram_fs_limit);

/* Clean resources allocated by fdcache library */
void fdc_deinit();

/**
 * @brief fdc_get_or_create create a new cache entry associated with the client
 *                        id `ino` or retrieve the entry if it already exists.
 * @param ino [IN] client inode number
 * @param block_size [IN] size of block, should be set to the size of the
 *                        buffers used to write data to the fd cache. (prefered
 *                        write size)
 * @param blocks_per_cluster [IN] number of blocks per cluster, should be set so
 *                        that the size of one cluster corresponds to the size
 *                        of buffers used to read data from the fd cache.
 *                        (prefered read size)
 * @param fd [OUT] on success, the value pointed to by fd will be set to the
 *                        opaque fd_cache pointer, used afterwards to read/write
 *                        to the fd cache. On error, its value is undefined
 * @return 0 on success, negative errno values on errors. Possible error codes:
 *	* -EINVAL for invalid arguments
 *	* -ENFILE if the maximum number of cache entries has been reached.
 */
int fdc_get_or_create(cache_ino_t ino,
		      size_t block_size,
		      size_t blocks_per_cluster,
		      fd_cache_t *fd);

/**
 * @brief fdc_write writes up to count bytes from the buffer starting at
 *                         buf to the cache entry fd, at offset offset. Required
 *                         number of clusters will be allocated.
 * @param fd cache entry opaque pointer
 * @param buf buffer to write
 * @param count number of bytes to write.
 * @param off offset from the cache entry start
 * @return the number of bytes written or a negative errno value to indicate an
 *                         error. Possible error codes:
 *	* -ENOMEM cluster can't be allocated
 *      * -EINVAL negative offset
 */
ssize_t fdc_write(fd_cache_t fd,
		  const void *buf,
		  size_t count,
		  off_t offset,
		  ssize_t *full_cluster);


/**
 * @brief fdc_read reads up to count bytes from the cache entry fd, at offset
 *                           offset, into the buffer starting at buf.
 * @param ent cache entry
 * @param cidx index of the cache entry cluster
 * @param buf buffer to read
 * @param count number of bytes to read
 * @param coff offset from the cluster start
 * @return the number of bytes read or a negative errno value to indicate an
 *         error. Possible error codes:
 *	* -EINVAL negative offset
 *	* -EOVERFLOW trying to read past the cluster end
 *	* -EFAULT trying to read unset memory (no such cluster was allocated)
 */
ssize_t fdc_read(fd_cache_t fd, void *buf, size_t count, off_t offset);

#endif
