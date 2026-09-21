#ifndef NEBULA_FS_H
#define NEBULA_FS_H

#include "types.h"

#define FS_NAME_MAX 32
#define FS_PATH_MAX 256

enum fs_type {
    FS_FILE,
    FS_DIR
};

struct fs_node {
    char             name[FS_NAME_MAX];
    enum fs_type     type;

    uint8_t         *data;        /* file contents; NULL for a directory */
    size_t           size;        /* bytes in use */
    size_t           capacity;    /* bytes allocated */

    struct fs_node  *parent;
    struct fs_node  *children;    /* first child, directories only */
    struct fs_node  *sibling;     /* next entry in the parent's list */

    uint32_t         created_ms;
};

void            fs_init(void);
struct fs_node *fs_root(void);

/*
 * Resolve a path against `base`. Absolute paths ignore base. Understands
 * "." and ".."; returns NULL if any component is missing.
 */
struct fs_node *fs_lookup(struct fs_node *base, const char *path);

/* Create a file or directory. Fails if the name is taken or the parent
 * does not exist. Returns NULL on failure. */
struct fs_node *fs_create(struct fs_node *base, const char *path, enum fs_type type);

/* Replace a file's contents. Returns false if it cannot grow the file. */
bool fs_write(struct fs_node *file, const void *data, size_t size);

/* Append to a file. */
bool fs_append(struct fs_node *file, const void *data, size_t size);

/* Remove a node. Refuses to remove a non-empty directory or the root. */
bool fs_unlink(struct fs_node *node);

/* Build the absolute path of a node into buf. */
void fs_path(struct fs_node *node, char *buf, size_t size);

/* Total bytes of file content across the whole tree. */
size_t fs_total_bytes(void);
uint32_t fs_node_count(void);

#endif /* NEBULA_FS_H */
