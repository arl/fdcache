#include <unistd.h>
#include <errno.h>
#include <jemalloc/jemalloc.h>
#include <glib.h>
#include <assert.h>
#include "fdcache.h"

// TODO: ADD LOCKING when touching at the cache entries!!!!

typedef struct fd_cache_entry_ {
	kvsns_ino_t ino;
	size_t total_size;
	size_t location; // RAM or filesystem
	union {
		struct {
			size_t bla1;
			size_t bla2;
		} fs;
		struct {
			size_t block_size; // max size of a contiguous memory block for this entry
			void *buffer;
		} ram;
	} u;

} fd_cache_entry_t;

/* for now this is quick and very dirty cache */
#define MAX_CACHE_ENTRIES 20
#define FREE_INODE ((kvsns_ino_t)-1)

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

void fdc_deinit()
{
	int i = 0;
	for (; i < MAX_CACHE_ENTRIES; i++) {
		fd_cache_entry_t *ent = &_fd_cache[i];
		if (ent->ino != FREE_INODE) {
			if (ent->location == IN_RAM_CACHE) {
				// TODO: continue
				free(ent->u.ram.buffer);
			} else {

			}
		}
	}
	memset(&_fd_cache, 0, sizeof(fd_cache_entry_t));
}

fd_cache_t fdc_get_or_create(kvsns_ino_t ino, size_t block_size)
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
	_fd_cache[i_free].u.ram.buffer = NULL;
	_fd_cache[i_free].u.ram.block_size = block_size;
	return (fd_cache_t)&_fd_cache[i_free];
}


//fd_cache_entry_t* fd_cache_alloc(kvsns_fd_t ino, size_t size)
//{
//	/* look for existing cache entry */
//	int i = 0;
//	for (; i < MAX_CACHE_ENTRIES; i++)
//	{
//		if (fd_cache[i].ino == ino) {
//			return &fd_cache[i];
//		}
//	}

//	/* create cache entry at the first free entry */
//	i = 0;
//	for (; i < MAX_CACHE_ENTRIES; i++)
//	{
//		if (fd_cache[i].ino == FREE_INODE) {
//			fd_cache[i].ino = ino;
//			return &fd_cache[i];
//		}
//	}

//	return NULL;
//}

ssize_t fdc_write(fd_cache_t fd, const void *buf, size_t count, off_t offset)
{
	fd_cache_entry_t *ent = (fd_cache_entry_t*)fd;

	// TODO: for size greater than multipart limit, do N allocations of the multipart size
	// TODO: for size greater then [N Megabytes], allocation is done on filesystem and not in RAM.

	if (ent->total_size < offset + count) {

		/* need to resize this cache entry */
		if (ent->location == IN_RAM_CACHE) {
			ent->u.ram.buffer = realloc(ent->u.ram.buffer, offset + count);

			if (!ent->u.ram.buffer) {
				/* instead of reallocating, we should log the
				 * error and move the content on filesystem
				 */
				return -ENOMEM;
			}
			/* successfully reallocated */
			ent->total_size = offset + count;
		} else {
			/* write bytes */
		}
	}

	/* perform copy */
	if (ent->location == IN_RAM_CACHE) {
		memcpy(ent->u.ram.buffer + offset, buf, count);
	} else {
		/* write bytes */
		assert(NULL); /*< not implemented */
	}

	return count;
}

ssize_t fdc_read(fd_cache_t fd, void *buf, size_t count, off_t offset)
{
	fd_cache_entry_t *ent = (fd_cache_entry_t*)fd;

	if (ent->location == IN_RAM_CACHE) {
		// data is in RAM

		// check we are not requesting an out-of-bounds offset
		if (offset >= ent->total_size)
			return -EFAULT;

		// TODO: when various mem block will be implemented, select the block

		// NOTE: with a very small size block and 'count' greater than the size block, we may require to memcpy multiple times to copy the whole buffer, also if it should pratically never happen.
		memcpy(buf, ent->u.ram.buffer + offset, count);
	} else {
		// not implemented
	}
	return count;
}
