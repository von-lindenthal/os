#include "fs.h"
#include "string.h"

struct file {
    int used;
    char name[FS_NAME_MAX];
    char data[FS_DATA_MAX];
    size_t len;
};

static struct file files[FS_MAX_FILES];

void fs_init(void)
{
    for (int i = 0; i < FS_MAX_FILES; i++) {
        files[i].used = 0;
        files[i].len = 0;
        files[i].name[0] = '\0';
    }

    fs_create("readme.txt");
    fs_write("readme.txt",
             "Welcome to os.\nType 'help' for commands.\n");
    fs_create("motd");
    fs_write("motd", "Have fun hacking the shell.\n");
}

static struct file *fs_find(const char *name)
{
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, name) == 0)
            return &files[i];
    }
    return 0;
}

static struct file *fs_alloc(void)
{
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!files[i].used)
            return &files[i];
    }
    return 0;
}

int fs_exists(const char *name)
{
    return fs_find(name) != 0;
}

int fs_create(const char *name)
{
    if (!is_valid_name(name, FS_NAME_MAX))
        return -1;
    if (fs_find(name))
        return 0;

    struct file *f = fs_alloc();
    if (!f)
        return -2;

    f->used = 1;
    strlcpy(f->name, name, FS_NAME_MAX);
    f->len = 0;
    f->data[0] = '\0';
    return 0;
}

int fs_write(const char *name, const char *data)
{
    if (!name || !data)
        return -1;
    struct file *f = fs_find(name);
    if (!f)
        return -1;

    size_t n = strlen(data);
    if (n >= FS_DATA_MAX)
        n = FS_DATA_MAX - 1;

    memcpy(f->data, data, n);
    f->data[n] = '\0';
    f->len = n;
    return 0;
}

int fs_append(const char *name, const char *data)
{
    if (!name || !data)
        return -1;
    struct file *f = fs_find(name);
    if (!f)
        return -1;
    if (f->len >= FS_DATA_MAX - 1)
        return 0;

    size_t add = strlen(data);
    size_t room = (FS_DATA_MAX - 1) - f->len;
    if (add > room)
        add = room;
    memcpy(f->data + f->len, data, add);
    f->len += add;
    f->data[f->len] = '\0';
    return 0;
}

int fs_read(const char *name, char *out, size_t out_size, size_t *out_len)
{
    struct file *f = fs_find(name);
    if (!f || out_size == 0)
        return -1;

    size_t n = f->len;
    if (n >= out_size)
        n = out_size - 1;
    memcpy(out, f->data, n);
    out[n] = '\0';
    if (out_len)
        *out_len = n;
    return 0;
}

int fs_remove(const char *name)
{
    struct file *f = fs_find(name);
    if (!f)
        return -1;
    f->used = 0;
    f->len = 0;
    f->name[0] = '\0';
    return 0;
}

int fs_rename(const char *old_name, const char *new_name)
{
    struct file *f = fs_find(old_name);
    if (!f)
        return -1;
    if (!is_valid_name(new_name, FS_NAME_MAX))
        return -2;
    if (fs_find(new_name))
        return -3;
    strlcpy(f->name, new_name, FS_NAME_MAX);
    return 0;
}

int fs_copy(const char *src, const char *dst)
{
    if (!src || !dst)
        return -1;
    struct file *f = fs_find(src);
    if (!f)
        return -1;
    if (fs_create(dst) < 0 && !fs_find(dst))
        return -2;
    return fs_write(dst, f->data);
}

void fs_list(void (*cb)(const char *name, size_t len))
{
    if (!cb)
        return;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (files[i].used)
            cb(files[i].name, files[i].len);
    }
}

int fs_count(void)
{
    int n = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (files[i].used)
            n++;
    }
    return n;
}

size_t fs_used_bytes(void)
{
    size_t n = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (files[i].used)
            n += files[i].len;
    }
    return n;
}

size_t fs_capacity_bytes(void)
{
    return (size_t)FS_MAX_FILES * FS_DATA_MAX;
}
