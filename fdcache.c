#include <unistd.h>
#include <errno.h>
#include <jemalloc/jemalloc.h>
#include <glib.h>
#include <assert.h>
#include <string.h>
#include "fdcache.h"
#include "bitmap.h"

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))


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

/* for now this is quick and very dirty cache
 * TODO: should be dynamic
 */
#define MAX_CACHE_ENTRIES 20
#define FREE_INODE ((cache_ino_t)-1)

#define IN_RAM_CACHE ((size_t)-1)

fd_cache_entry_t _fd_cache[MAX_CACHE_ENTRIES];
size_t _ram_fs_limit;

gint _key_cmp (gconstpointer a, gconstpointer b);

void fdc_init(size_t ram_fs_limit)
{
	memset(&_fd_cache, 0, sizeof(fd_cache_entry_t));
	int i = 0;
	for (; i < MAX_CACHE_ENTRIES; i++)
		_fd_cache[i].ino = FREE_INODE;
	_ram_fs_limit = ram_fs_limit;
}

void fdc_deinit()
{
	int i = 0;
	for (; i < MAX_CACHE_ENTRIES; i++) {
		fd_cache_entry_t *ent = &_fd_cache[i];
		if (ent->ino != FREE_INODE) {
			if (ent->location == IN_RAM_CACHE) {
				// TODO: continue
//				free(ent->u.ram.buffer);
			} else {

			}
		}
	}
	memset(&_fd_cache, 0, sizeof(fd_cache_entry_t));
}

/**
 * @brief __fdc_lookup look for a specific client inode
 * @param ino
 * @param free_idx set to the first free index if found, or negative if no
 *                   free cache entry has been found during the lookup
 * @return the cache entry or NULL if not found
 */
fd_cache_entry_t * __fdc_lookup(cache_ino_t ino, int *free_idx)
{
	int i = 0;
	*free_idx = -1;
	for (; i < MAX_CACHE_ENTRIES; i++) {
		if (_fd_cache[i].ino == ino)
			return &_fd_cache[i];
		else if (*free_idx == -1 && _fd_cache[i].ino == FREE_INODE)
			*free_idx = i;
	}
	return NULL;
}

int fdc_get_or_create(
		cache_ino_t ino,
		size_t block_size,
		size_t blocks_per_cluster,
		fd_cache_t *fd)
{
	/* check arguments */
	if (!block_size || !blocks_per_cluster || !fd) {
		return -EINVAL;
	}

	/* look for existing cache entry */
	fd_cache_entry_t * ent;
	int free_idx = -1;
	ent = __fdc_lookup(ino, &free_idx);
	if (ent) {
		 *fd = (fd_cache_t) ent;
		return 0;
	}

	/* create cache entry at the first free entry */
	if (free_idx == -1)
		return -ENFILE;

	/* create new cache entry, in ram and empty */
	ent = &_fd_cache[free_idx];
	ent->ino = ino;
	ent->total_size = 0;
	ent->location = IN_RAM_CACHE;
	ent->block_size = block_size;
	ent->blocks_per_cluster = blocks_per_cluster;
	ent->u.ram.buf_map = g_tree_new (_key_cmp);
	ent->bitmap = 0; /* bitmap will be allocated at first write */

	*fd = (fd_cache_t) ent;
	return 0;
}
	return 0;
}

/**
 * @brief _fdc_ram_cluster_write writes up to count bytes from the buffer
 *                         starting at buf to the cluster represented by cidx,
 *                         at offset coff.
 * @param ent cache entry
 * @param cidx index of the cache entry cluster. Allocate the whole cluster if
 *                         it's not allocated yet
 * @param buf buffer to write
 * @param count number of bytes to write
 * @param coff offset from the cluster start
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
			       off_t coff)
{
	const size_t cluster_size = ent->block_size * ent->blocks_per_cluster;
	if (coff < 0 || coff > cluster_size)
		return -EINVAL;
	if (count + coff > cluster_size)
		return -EOVERFLOW;

	/* retrieve the memory region corresponding to the cluster */
	void *clusterbuf = g_tree_lookup(ent->u.ram.buf_map, (gpointer*) cidx);
	if (clusterbuf == NULL) {
		/* TODO: if the file is currently made of 1 cluster only
		 * (check with cidx and ent->total_size, we don't need to
		 * allocate the full cluster */

		/* allocate the cluster */
		clusterbuf = malloc(sizeof(cluster_size));
		if (!clusterbuf) {
			return -ENOMEM;
		} else {
			g_tree_insert(ent->u.ram.buf_map, (gpointer*) cidx, clusterbuf);
		}
	}
	memcpy(clusterbuf + coff, buf, count);
	return count;
}

ssize_t fdc_write(fd_cache_t fd,
		  const void *buf,
		  size_t count,
		  off_t offset,
		  ssize_t *full_cluster)
{
	fd_cache_entry_t *ent = (fd_cache_entry_t*)fd;

	const size_t cluster_size = ent->block_size * ent->blocks_per_cluster;
	const size_t last_offset = offset + count;
	ssize_t rc;
	size_t nwritten = 0;

	/* if it's the first write, we must allocate the bitmap */
	if (!ent->bitmap) {
		ent->bitmap = bitmap_alloc(DIV_ROUND_UP(last_offset, ent->block_size));
		bitmap_zero(ent->bitmap);
	}

	if (ent->location == IN_RAM_CACHE) {
		/* compute indices of first and last clusters to write to */
		size_t cidx = offset / cluster_size;
		const size_t last_cidx = last_offset / cluster_size;

		/* compute offset for first cluster to write to */
		off_t coff = offset % cluster_size;

		/* number of bytes remaining to be written */
		size_t nremain = count;
		size_t ccount; /* number of bytes to write to current cluster */

		for (; cidx <= last_cidx; ++cidx) {

			/* compute count for current cluster */
			ccount = cluster_size - coff > nremain ? nremain : cluster_size - coff;

			printf("_fdc_ram_cluster_write: cidx=%lu buf=buf+0x%lu ccount=%lu coff=%lu\n",
			       cidx, last_offset - nremain, ccount, coff);

			rc = _fdc_ram_cluster_write(ent, cidx, buf + (count - nremain), ccount, coff);
			if (rc < 0)
				return rc;
			nwritten += rc;

			/* prepare for writing to next cluster */
			coff = 0;
			nremain -= ccount;

			if (nremain == 0)
				break;
		}
		/* update total size */
		if (ent->total_size < last_offset)
			ent->total_size = last_offset;
	} else {
		/* directly write to filesystem */
	}

	return nwritten;
}

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
			      off_t coff)
{
	const size_t cluster_size = ent->block_size * ent->blocks_per_cluster;
	if (coff < 0 || coff > cluster_size)
		return -EINVAL;
	if (count + coff > cluster_size)
		return -EOVERFLOW;

	/* retrieve the memory region corresponding to the cluster */
	void *clusterbuf = g_tree_lookup(ent->u.ram.buf_map, (gpointer*) cidx);
	if (clusterbuf == NULL) {
		return -EFAULT;
	}
	memcpy(buf, clusterbuf + coff, count);
	return count;
}

ssize_t fdc_read(fd_cache_t fd,
		 void *buf,
		 size_t count,
		 off_t offset)
{
	fd_cache_entry_t *ent = (fd_cache_entry_t*)fd;

	const size_t cluster_size = ent->block_size * ent->blocks_per_cluster;
	const size_t last_offset = offset + count > ent->total_size ?
				   ent->total_size : offset + count;
	size_t nread = 0;
	ssize_t rc;

	if (ent->location == IN_RAM_CACHE) {
		if (offset < ent->total_size) {
			/* compute indices of first and last clusters to read from */
			size_t cidx = offset / cluster_size;
			const size_t last_cidx = last_offset / cluster_size;

			/* compute offset for first cluster to read from */
			off_t coff = offset % cluster_size;

			/* number of bytes remaining to read */
			size_t nremain = count;
			size_t ccount; /* number of bytes to read from current cluster */

			for (; cidx <= last_cidx; ++cidx) {

				/* compute count for current cluster */
				ccount = cluster_size - coff > nremain ? nremain : cluster_size - coff;

				printf("_fdc_ram_cluster_read: cidx=%lu buf=buf+0x%lu ccount=%lu coff=%lu\n",
				       cidx, last_offset - nremain, ccount, coff);

				rc = _fdc_ram_cluster_read(ent, cidx, buf + (count - nremain), ccount, coff);
				if (rc < 0)
					return rc;
				nread += rc;

				/* prepare for writing to next cluster */
				coff = 0;
				nremain -= ccount;

				if (nremain == 0)
					break;
			}

		} else if (offset == ent->total_size) {
			/* as pread, 0 means end-of-file and is not an error */
			return 0;

		} else if (offset > ent->total_size) {
			/* offset out of bounds */
			return -EFAULT;
		}
	} else {
		/* not implemented */
		assert(0);
	}
	return nread;
}

gint _key_cmp (gconstpointer a, gconstpointer b)
{
	if (a < b)
		return -1;
	else if (a > b)
		return 1;
	return 0;
}

