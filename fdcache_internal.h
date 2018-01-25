#ifndef FDCACHE_INTERNAL_H
#define FDCACHE_INTERNAL_H

#include <glib.h>
#include "bitmap.h"
#include "fdcache.h"

/* for now this is quick and very dirty cache
 * TODO: should be dynamic
 */
#define MAX_CACHE_ENTRIES 20

//
// TODO: ADD LOCKING when touching at the cache entries!!!!

typedef struct fd_cache_entry_ {
	cache_ino_t ino;
	size_t total_size;
	size_t block_size;
	size_t blocks_per_cluster;
	bitmap_hdl bitmap;
	size_t location;		/* RAM or filesystem */
	union {
		struct {
			size_t bla1;
			size_t bla2;
		} fs;
		struct {
			GTree *buf_map;	/* key: cluster index value: cluster buffer */
		} ram;
	} u;

} fd_cache_entry_t;

gint _key_cmp (gconstpointer a, gconstpointer b);

/**
 * @brief __fdc_lookup look for a specific client inode
 * @param ino
 * @param free_idx set to the first free index if found, or negative if no
 *                   free cache entry has been found during the lookup
 * @return the cache entry or NULL if not found
 */
fd_cache_entry_t * __fdc_lookup(cache_ino_t ino, int *free_idx);

/**
 * @brief _fdc_ram_cluster_write writes up to count bytes from the buffer
 *                         starting at buf to the cluster represented by cidx,
 *                         at offset coff. Only for entries located in RAM.
 * @param ent cache entry
 * @param cidx index of the cache entry cluster. Allocate the whole cluster if
 *                         it's not allocated yet
 * @param buf buffer to write
 * @param count number of bytes to write
 * @param coff offset from the cluster start
 * @param unique_cluster the whole entry holds on a single cluster
 * @return the number of bytes written or a negative errno value to indicate an
 *                         error. Possible error codes:
 *	* -ENOMEM cluster can't be allocated
 *      * -EINVAL invalid offset (negative or greater than cluster size)
 *	* -EOVERFLOW trying to write past the cluster end
 */
ssize_t _fdc_ram_cluster_write(fd_cache_entry_t *ent,
			       size_t cidx,
			       const void *buf,
			       size_t count,
			       off_t coff,
			       bool unique_cluster);

/**
 * @brief _fdc_ram_cluster_read reads up to count bytes from the cluster
 *                           represented by cid, at offset coff, into the buffer
 *                           starting at buf.
 * @param ent cache entry
 * @param cidx index of the cache entry cluster
 * @param buf buffer to read
 * @param count number of bytes to read
 * @param coff offset from the cluster start
 * @return the number of bytes read or a negative errno value to indicate an
 *         error. Possible error codes:
 *	* -EFAULT cluster not allocated/not found
 *      * -EINVAL invalid offset (negative or greater than cluster size)
 *	* -EOVERFLOW trying to read past the cluster end
 */
ssize_t _fdc_ram_cluster_read(fd_cache_entry_t *ent,
		              size_t cidx,
			      void *buf,
			      size_t count,
			      off_t coff);

#endif
