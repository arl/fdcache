#include <stdio.h>
#include <string.h>

/* inocache.h file */

typedef unsigned long long int kvsns_ino_t;

/* users don't know about fd_cache_entry */
typedef void* fd_cache_t;

/*
 * returns cache entry for specified inode or NULL if it doesn't exist.
 */
void fdc_init(size_t ram_fs_limit);

void fdc_deinit();


fd_cache_t fdc_get_or_create(kvsns_ino_t ino, size_t block_size);

// will return 0 or positive or negative error codes
ssize_t fdc_write(fd_cache_t fd, const void *buf, size_t count, off_t offset);

// will return 0 or positive or negative error codes
ssize_t fdc_read(fd_cache_t fd, void *buf, size_t count, off_t offset);
