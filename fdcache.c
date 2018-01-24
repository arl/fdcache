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
	kvsns_ino_t ino;
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

/* for now this is quick and very dirty cache */
#define MAX_CACHE_ENTRIES 20
#define FREE_INODE ((kvsns_ino_t)-1)

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

int fdc_get_or_create(kvsns_ino_t ino, size_t block_size, size_t blocks_per_cluster, fd_cache_t *fd)
{
	/* look for existing cache entry */
	int i = 0;
	int i_free = -1;
	for (; i < MAX_CACHE_ENTRIES; i++)
	{
		if (_fd_cache[i].ino == ino)
			return (fd_cache_t)&_fd_cache[i];
		else if (i_free == -1 && _fd_cache[i].ino == FREE_INODE)
			i_free = i;
	}

	/* create cache entry at the first free entry */
	if (i_free == -1)
		return NULL;

	/* create new cache entry, in ram and empty */
	_fd_cache[i_free].ino = ino;
	_fd_cache[i_free].total_size = 0;
	_fd_cache[i_free].location = IN_RAM_CACHE;
	_fd_cache[i_free].block_size = block_size;
	_fd_cache[i_free].blocks_per_cluster = blocks_per_cluster;
	_fd_cache[i_free].u.ram.buf_map = g_tree_new (_key_cmp);
	_fd_cache[i_free].bitmap = 0; /* bitmap will be allocated at first write */

	return (fd_cache_t)&_fd_cache[i_free];
}

/*
 * Write a buffer to a specific cluster
 * cidx cluster index
 * buf buffer to write
 * count number of bytes to write from buffer to the cluster
 * coff offset indicating where to start writing in the cluster buffer
 *
 * undefined behaviour when trying to write past the cluster boundary
 */
void _fdc_ram_cluster_write(fd_cache_entry_t *ent, size_t cidx, const void *buf, size_t count, off_t coff)
{
	const size_t cluster_size = ent->block_size * ent->blocks_per_cluster;
	assert(count + coff <= cluster_size);

	/* retrieve the memory region corresponding to the cluster */
	void *clusterbuf = g_tree_lookup(ent->u.ram.buf_map, (gpointer*) cidx);
	if (clusterbuf == NULL) {
		/* TODO: if the file is currently made of 1 cluster only
		 * (check with cidx and ent->total_size, we don't need to
		 * allocate the full cluster */

		/* allocate the cluster */
		clusterbuf = malloc(sizeof(cluster_size));
		g_tree_insert(ent->u.ram.buf_map, (gpointer*) cidx, clusterbuf);
	}
	memcpy(clusterbuf + coff, buf, count);
}

ssize_t fdc_write(fd_cache_t fd, const void *buf, size_t count, off_t offset, ssize_t *full_cluster)
{
	fd_cache_entry_t *ent = (fd_cache_entry_t*)fd;

	const size_t cluster_size = ent->block_size * ent->blocks_per_cluster;
	const size_t last_offset = offset + count;

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

			printf("_fdc_ram_cluster_write: cidx=%lu buf=buf+0x%lu ccount=%lu coff=%lu\n", cidx, last_offset - nremain, ccount, coff);

			_fdc_ram_cluster_write(ent, cidx, buf + (count - nremain), ccount, coff);

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

	/* TODO return value */
}

/*
 * Read a buffer from a specific cluster
 * cidx cluster index
 * buf buffer to read into
 * count number of bytes to read from the cluster into the buffer
 * coff offset indicating where to start reading from the cluster
 *
 * undefined behaviour when trying to read past the cluster boundary
 *
 * return negative value in case of error, count in case of success
 */
ssize_t _fdc_ram_cluster_read(fd_cache_entry_t *ent, size_t cidx, void *buf, size_t count, off_t coff)
{
	const size_t cluster_size = ent->block_size * ent->blocks_per_cluster;
	assert(count + coff <= cluster_size);

	/* retrieve the memory region corresponding to the cluster */
	void *clusterbuf = g_tree_lookup(ent->u.ram.buf_map, (gpointer*) cidx);
	if (clusterbuf == NULL) {
		return -EFAULT;
	}
	memcpy(buf, clusterbuf + coff, count);
	return count;
}

ssize_t fdc_read(fd_cache_t fd, void *buf, size_t count, off_t offset)
{
	fd_cache_entry_t *ent = (fd_cache_entry_t*)fd;

	const size_t cluster_size = ent->block_size * ent->blocks_per_cluster;
	const size_t last_offset = offset + count > ent->total_size ? ent->total_size : offset + count;
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

				printf("_fdc_ram_cluster_read: cidx=%lu buf=buf+0x%lu ccount=%lu coff=%lu\n", cidx, last_offset - nremain, ccount, coff);

				rc = _fdc_ram_cluster_read(ent, cidx, buf + (count - nremain), ccount, coff);
				if (rc< 0)
					return -EFAULT;

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
	return count;
}

gint _key_cmp (gconstpointer a, gconstpointer b)
{
	if (a < b)
		return -1;
	else if (a > b)
		return 1;
	return 0;
}

