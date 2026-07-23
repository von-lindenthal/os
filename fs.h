#ifndef FS_H
#define FS_H

#include <stddef.h>

#define FS_MAX_FILES 16
#define FS_NAME_MAX  28
#define FS_DATA_MAX  512

void fs_init(void);
int fs_create(const char *name);
int fs_write(const char *name, const char *data);
int fs_append(const char *name, const char *data);
int fs_read(const char *name, char *out, size_t out_size, size_t *out_len);
int fs_remove(const char *name);
int fs_rename(const char *old_name, const char *new_name);
int fs_copy(const char *src, const char *dst);
int fs_exists(const char *name);
void fs_list(void (*cb)(const char *name, size_t len));
int fs_count(void);
size_t fs_used_bytes(void);
size_t fs_capacity_bytes(void);

#endif
