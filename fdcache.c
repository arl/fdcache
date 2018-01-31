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

gboolean _buf_map_free_page(gpointer page_idx,
			    gpointer page_buf,
			    gpointer data)
{
	/* free the memory page */
	free((void*) page_buf);
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
					roaring_bitmap_free(ent->bitmap);
				ent->bitmap = NULL;

				/* free allocated pages */
				g_tree_foreach(ent->u.ram.buf_map,
					       _buf_map_free_page,
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
		size_t page_size,
		fd_cache_t *fd)
{
	/* check arguments */
	if (!page_size || !fd) {
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
	ent->page_size = page_size;
	ent->u.ram.buf_map = g_tree_new (_key_cmp);
	ent->bitmap = roaring_bitmap_create();

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
		if (ent->total_size <= ent->page_size) {
			/* special case, entry holds on a single page */
			*nbytes = ent->total_size;
		} else {
			/* count the number of allocated pages */
			size_t npages = g_tree_nnodes(ent->u.ram.buf_map);
			*nbytes = npages * ent->page_size;
		}
	} else {
		/* Not implemented ! */
		assert(0);
	}

	return 0;
}

ssize_t _fdc_ram_page_write(fd_cache_entry_t *ent,
			       size_t page_idx,
			       const void *buf,
			       size_t count,
			       off_t page_off,
			       bool unique_page)
{
	if (page_off < 0 || page_off > ent->page_size)
		return -EINVAL;
	if (count + page_off > ent->page_size)
		return -EOVERFLOW;

	/* retrieve the address of this page */
	void *page_buf = g_tree_lookup(ent->u.ram.buf_map, (gpointer*) page_idx);
	if (page_buf == NULL) {
		/* allocate the page memory. If the entry is made of a single
		 * page, we just allocate a buffer of the required size, and not
		 * a whole page. */
		page_buf = malloc(unique_page ? count + page_off : ent->page_size);
		if (!page_buf)
			return -ENOMEM;
		g_tree_insert(ent->u.ram.buf_map, (gpointer*) page_idx, page_buf);
	}
	if (unique_page && (count + page_off > ent->total_size)) {
		/* in case of single page entry, we may need to realloc if
		 * not enough memory was allocated in previous writes */
		void *newpage_buf = realloc(page_buf, count + page_off);
		if (newpage_buf != page_buf) {
			/* memory was moved, update the buffer tree */
			g_tree_insert(ent->u.ram.buf_map, (gpointer*) page_idx, newpage_buf);
		}
	}
	memcpy(page_buf + page_off, buf, count);
	return count;
}

ssize_t fdc_write(fd_cache_t fd,
		  const void *buf,
		  size_t count,
		  off_t offset,
		  ssize_t *full_page)
{
	fd_cache_entry_t *ent = (fd_cache_entry_t*)fd;

	const size_t last_offset = offset + count;
	ssize_t rc;
	size_t nwritten = 0;

	if (ent->location == IN_RAM_CACHE) {
		/* compute indices of first and last pages to write to */
		size_t page_idx = offset / ent->page_size;
		const size_t lastpage_idx = last_offset / ent->page_size;

		/* compute offset for the first page to write to */
		off_t page_off = offset % ent->page_size;

		/* number of bytes remaining to be written */
		size_t nremain = count;
		size_t page_count; /* number of bytes to write to current page */

		for (; page_idx <= lastpage_idx; ++page_idx) {

			/* compute count for current page */
			page_count = ent->page_size - page_off > nremain ? nremain : ent->page_size - page_off;

			printf("_fdc_ram_page_write: page_idx=%lu buf=buf+0x%lu page_count=%lu page_off=%lu\n",
			       page_idx, last_offset - nremain, page_count, page_off);

			rc = _fdc_ram_page_write(ent, page_idx, buf + (count - nremain), page_count, page_off, lastpage_idx == 0);
			if (rc < 0)
				return rc;
			nwritten += rc;

			/* prepare for writing to next page */
			page_off = 0;
			nremain -= page_count;

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

ssize_t _fdc_ram_page_read(fd_cache_entry_t *ent,
		              size_t page_idx,
			      void *buf,
			      size_t count,
			      off_t page_off)
{
	if (page_off < 0 || page_off > ent->page_size)
		return -EINVAL;
	if (count + page_off > ent->page_size)
		return -EOVERFLOW;

	/* retrieve the page */
	void *page_buf = g_tree_lookup(ent->u.ram.buf_map, (gpointer*) page_idx);
	if (page_buf == NULL) {
		return -EFAULT;
	}
	memcpy(buf, page_buf + page_off, count);
	return count;
}

ssize_t fdc_read(fd_cache_t fd,
		 void *buf,
		 size_t count,
		 off_t offset)
{
	fd_cache_entry_t *ent = (fd_cache_entry_t*)fd;

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
			/* compute indices of first and last pages to read from */
			size_t cidx = offset / ent->page_size;
			const size_t last_cidx = last_offset / ent->page_size;

			/* compute offset for first page to read from */
			off_t coff = offset % ent->page_size;

			/* number of bytes remaining to read */
			size_t nremain = count;
			size_t page_count; /* number of bytes to read from current page */

			for (; cidx <= last_cidx; ++cidx) {

				/* compute count for current page */
				page_count = ent->page_size - coff > nremain ? nremain : ent->page_size - coff;

				printf("_fdc_ram_page_read: cidx=%lu buf=buf+0x%lu page_count=%lu coff=%lu\n",
				       cidx, last_offset - nremain, page_count, coff);

				rc = _fdc_ram_page_read(ent, cidx, buf + (count - nremain), page_count, coff);
				if (rc < 0)
					return rc;
				nread += rc;

				/* prepare for writing to next page */
				coff = 0;
				nremain -= page_count;

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

