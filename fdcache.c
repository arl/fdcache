#include <unistd.h>
#include <errno.h>
#include <jemalloc/jemalloc.h>
#include <glib.h>
#include <assert.h>
#include "fdcache.h"

// TODO: ADD LOCKING when touching at the cache entries!!!!

/*
//ex-garray-7.c
#include <glib.h>
#include <stdio.h>
int main(int argc, char** argv) {
 GPtrArray* a = g_ptr_array_new();
 g_ptr_array_add(a, g_strdup("hello "));
 g_ptr_array_add(a, g_strdup("again "));
 g_ptr_array_add(a, g_strdup("there "));
 g_ptr_array_add(a, g_strdup("world "));
 g_ptr_array_add(a, g_strdup("\n"));
 printf(">Here are the GPtrArray contents\n");
 g_ptr_array_foreach(a, (GFunc)printf, NULL);
 printf(">Removing the third item\n");
 g_ptr_array_remove_index(a, 2);
 g_ptr_array_foreach(a, (GFunc)printf, NULL);
 printf(">Removing the second and third item\n");
 g_ptr_array_remove_range(a, 1, 2);
 g_ptr_array_foreach(a, (GFunc)printf, NULL);
 printf("The first item is '%s'\n", g_ptr_array_index(a, 0));
 g_ptr_array_free(a, TRUE);
 return 0;
}
*/



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
			size_t blk_size; // max size of a contiguous memory block for this entry
			size_t num_blks; // number of currently allocated blocks (the last block may not be complete)
			GPtrArray *blks; // pointer to the array of blocks
//			void *buffer;
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
//				free(ent->u.ram.buffer);
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
//	_fd_cache[i_free].u.ram.buffer = NULL;
	_fd_cache[i_free].u.ram.blk_size = block_size;
	_fd_cache[i_free].u.ram.num_blks = 0;
	_fd_cache[i_free].u.ram.blks = g_ptr_array_new_with_free_func ((GDestroyNotify)free);

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

//	we should never receive a count bigger than the blok size, but it may happen, and anyway we may receive
//	count-offset pairs that overwrite 2 blocks

	// TODO: for size greater than multipart limit, do N allocations of the multipart size
	// TODO: for size greater then [N Megabytes], allocation is done on filesystem and not in RAM.

	size_t req_size = offset + count;
	void *blk = NULL;

	if (ent->location == IN_RAM_CACHE) {

		if (ent->total_size < req_size) {

			/* need to resize this cache entry */
			size_t missing = req_size - ent->total_size;
			size_t last_blk_size = (ent->u.ram.num_blks * ent->u.ram.blk_size) - ent->total_size;
			if (last_blk_size + missing <= ent->u.ram.blk_size) {
				/* only the last block needs to be reallocated */

				if (!ent->u.ram.num_blks) {
					blk = malloc(count % ent->u.ram.blk_size);
					g_ptr_array_add (ent->u.ram.blks, blk);
					ent->u.ram.num_blks = 1;
				} else {
					blk = g_ptr_array_index(ent->u.ram.blks, ent->u.ram.blk_size - 1);
					realloc(blk, missing);
				}
			} else {
				/* we need to fill the last block and add some more block(s) */

				/* fill last block */
				blk = g_ptr_array_index(ent->u.ram.blks, ent->u.ram.blk_size - 1);
				realloc(blk, ent->u.ram.blk_size - last_blk_size);

				/* adjust count of missing bytes */
				missing -= ent->u.ram.blk_size - last_blk_size;
				size_t new_num_blk = missing / ent->u.ram.blk_size;
				last_blk_size = new_num_blk * ent->u.ram.blk_size;
				if (last_blk_size)
					/* will need another, non full, block */
					new_num_blk++;
				g_ptr_array_set_size(ent->u.ram.blks, new_num_blk);
				for (size_t i = ent->u.ram.num_blks; i < new_num_blk - 1; i++) {
					blk = malloc(ent->u.ram.blk_size);
					g_ptr_array_add (ent->u.ram.blks, blk);
					ent->u.ram.num_blks++;
				}
				blk = malloc(last_blk_size);
				g_ptr_array_add (ent->u.ram.blks, blk);
				ent->u.ram.num_blks++;
			}

			/* successfully reallocated */
			ent->total_size = req_size;

			/* now we can write */
			size_t blk_off = offset % ent->u.ram.blk_size;
			size_t cur_blk_idx = (offset - blk_off) / ent->u.ram.blk_size;
			size_t last_blk_idx = (offset - (offset % ent->u.ram.blk_size)) / ent->u.ram.blk_size;
			size_t to_be_written = count;	// bytes to be written
			size_t blk_end_off;		// end offset for current block
			for (; cur_blk_idx <= last_blk_idx; cur_blk_idx++) {
				blk = g_ptr_array_index(ent->u.ram.blks, cur_blk_idx);
				blk_end_off = blk_off + to_be_written <= ent->u.ram.blk_size ? blk_off + to_be_written : ent->u.ram.blk_size;
				memcpy(blk + blk_off, buf + (count - to_be_written), blk_end_off - blk_off);
				to_be_written -= blk_end_off - blk_off;
				blk_off = 0;



//				if (to_be_written >= ent->u.ram.blk_size - blk_off) {
//					/* there's more bytes to be written than current block contains */
//					memcpy(blk + blk_off, buf + (count - to_be_written), ent->u.ram.blk_size - blk_off);
//					to_be_written -= ent->u.ram.blk_size - blk_off;
//					blk_off = 0;
//				} else {
//					/* bytes to be written don't span more than current block */
//					memcpy(blk, buf + (count - to_be_written), ent->u.ram.blk_size - blk_off);
//					to_be_written -= ent->u.ram.blk_size - blk_off;
//					blk_off = 0;

//				}
//				blk_end_off =
//				if (to_be_written > ent->u.ram.blk_size - blk_off) {
//					/* */
//				}
//				blk_end_off = to_be_written > ent->u.ram.blk_size - blk_off ? ent->u.ram.blk_size - blk_off
//				memcpy(blk + blk_off, buf, to_be_written > ent->u.ram.blk_size - blk_off ? ent->u.ram.blk_size - blk_off);
//				blk_off = 0;
//				g_ptr_array_add (ent->u.ram.blks, blk);
//				ent->u.ram.num_blks++;
			}



		} else {
		}
	} else {
		/* write bytes to filesystem */
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
//		memcpy(buf, ent->u.ram.buffer + offset, count);
	} else {
		// not implemented
	}
	return count;
}
