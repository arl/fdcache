#include <unistd.h>
#include <errno.h>
#include <jemalloc/jemalloc.h>
#include <assert.h>
#include <string.h>
#include "fdcache_internal.h"

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

// TODO: ADD LOCKING when touching at the cache entries!!!!
#define FREE_INODE ((cache_ino_t)-1)
#define IN_RAM_CACHE ((size_t)-1)


fd_cache_entry_t _fd_cache[MAX_CACHE_ENTRIES];
size_t _ram_fs_limit;

void fdc_init(size_t ram_fs_limit)
{
	memset(&_fd_cache, 0, sizeof(fd_cache_entry_t));
	int i = 0;
	for (; i < MAX_CACHE_ENTRIES; i++)
		_fd_cache[i].ino = FREE_INODE;
	_ram_fs_limit = ram_fs_limit;
}

gboolean _buf_map_free_cluster(gpointer cidx,
			       gpointer cbuf,
			       gpointer data)
{
	/* free the cluster */
	free((void*) cbuf);
	return FALSE;
}

void fdc_deinit()
{
	int i = 0;
	for (; i < MAX_CACHE_ENTRIES; i++) {
		fd_cache_entry_t *ent = &_fd_cache[i];
		if (ent->ino != FREE_INODE) {
			if (ent->location == IN_RAM_CACHE) {
				if (ent->bitmap)
					bitmap_free(ent->bitmap);

				/* free allocated clusters */
				g_tree_foreach(ent->u.ram.buf_map,
					       _buf_map_free_cluster,
					       NULL);
				g_tree_destroy(ent->u.ram.buf_map);
				ent->u.ram.buf_map = NULL;
			} else {
				/* TODO: to implement */
			}
		}
	}
	memset(&_fd_cache, 0, sizeof(fd_cache_entry_t));
}

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

int fdc_entry_size(cache_ino_t ino, size_t *nbytes)
{
	/* look for existing cache entry */
	fd_cache_entry_t * ent;
	int free_idx = -1;
	ent = __fdc_lookup(ino, &free_idx);
	if (!ent)
		return -EFAULT;
	*nbytes = ent->total_size;
	return 0;
}

int fdc_entry_mem(cache_ino_t ino, size_t *nbytes)
{
	/* look for existing cache entry */
	fd_cache_entry_t * ent;
	int free_idx = -1;
	ent = __fdc_lookup(ino, &free_idx);
	if (!ent)
		return -EFAULT;

	if (ent->location == IN_RAM_CACHE) {
		/* count the number of allocated clusters */
		size_t nclusters = g_tree_nnodes(ent->u.ram.buf_map);
		*nbytes = nclusters * ent->block_size * ent->blocks_per_cluster;
	} else {
		/* Not implemented ! */
		assert(0);
	}

	return 0;
}

int _fdc_ram_write_block(fd_cache_entry_t *ent,
			       size_t cidx,
			       size_t bidx,
			       const void *buf)
{
	const size_t cluster_size = ent->block_size * ent->blocks_per_cluster;

	/* retrieve or allocate the cluster  */
	void *cbuf = g_tree_lookup(ent->u.ram.buf_map, (gpointer*) cidx);
	if (cbuf == NULL) {
		cbuf = malloc(cluster_size);
		if (!cbuf)
			return -ENOMEM;
		g_tree_insert(ent->u.ram.buf_map, (gpointer*) cidx, cbuf);
	}
	memcpy(cbuf + bidx * ent->block_size, buf, ent->block_size);
	return 0;
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

	/* check the buffer is block-size aligned */
	if ((offset % ent->block_size) || (count % ent->block_size)) {
		/* illegal seek */
		return -ESPIPE;
	}

	/* if it's the first write, we must allocate the bitmap */
	if (!ent->bitmap) {
		ent->bitmap = bitmap_alloc(DIV_ROUND_UP(last_offset, ent->block_size));
		bitmap_zero(ent->bitmap);
	}

	if (ent->location == IN_RAM_CACHE) {
		size_t curoff = (size_t) offset;
		size_t cidx, bidx;
		while (curoff < offset + count) {

			cidx = curoff / cluster_size;
			bidx = (curoff % cluster_size) / ent->block_size;

			printf("_fdc_ram_write_block: cidx=%lu bidx=%lu\n", cidx, bidx);

			rc = _fdc_ram_write_block(ent, cidx, bidx, buf + nwritten);
			if (rc < 0)
				return rc;
			curoff += ent->block_size;
			nwritten += ent->block_size;
		}
		/* update total size */
		if (ent->total_size < offset + count)
			ent->total_size = offset + count;

	} else {
		/* directly write to filesystem */
	}

	return nwritten;
}

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

	if (offset < 0 || offset > ent->total_size)
		return -EINVAL;

	if (count + offset > ent->total_size)
		return -EOVERFLOW;

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

