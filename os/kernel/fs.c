/*
 * fs.c -- a small in-memory filesystem.
 *
 * The tree is ordinary heap-allocated nodes: each directory keeps a
 * singly linked list of children, each node a pointer back to its
 * parent. File contents live in a separate heap buffer that grows
 * geometrically, so repeated appends do not turn into repeated copies.
 *
 * There is no backing store -- the disk driver this would need is beyond
 * the boot loader's single-sector reads -- so everything lives and dies
 * with the running kernel.
 */

#include "fs.h"
#include "heap.h"
#include "string.h"
#include "kprintf.h"
#include "timer.h"

static struct fs_node *root;
static uint32_t        node_count;

struct fs_node *fs_root(void)
{
    return root;
}

uint32_t fs_node_count(void)
{
    return node_count;
}

static struct fs_node *node_new(const char *name, enum fs_type type)
{
    struct fs_node *node = kcalloc(1, sizeof(struct fs_node));

    if (!node)
        return NULL;

    strlcpy(node->name, name, FS_NAME_MAX);
    node->type       = type;
    node->created_ms = timer_uptime_ms();
    node_count++;

    return node;
}

/* Find a direct child by name. */
static struct fs_node *find_child(struct fs_node *dir, const char *name,
                                  size_t name_len)
{
    if (!dir || dir->type != FS_DIR)
        return NULL;

    for (struct fs_node *child = dir->children; child; child = child->sibling) {
        if (strlen(child->name) == name_len &&
            strncmp(child->name, name, name_len) == 0)
            return child;
    }

    return NULL;
}

/*
 * Walk a path one component at a time.
 *
 * When `parent_only` is set we stop before the final component and hand
 * it back through `last`/`last_len`, which is what create needs: the
 * directory to insert into, plus the name to insert.
 */
static struct fs_node *walk(struct fs_node *base, const char *path,
                            bool parent_only,
                            const char **last, size_t *last_len)
{
    struct fs_node *node = (path[0] == '/') ? root : (base ? base : root);

    if (path[0] == '/')
        path++;

    for (;;) {
        /* Skip any run of separators, including a trailing one. */
        while (*path == '/')
            path++;

        if (*path == '\0') {
            if (parent_only)
                return NULL;    /* nothing left to name */
            return node;
        }

        const char *start = path;
        while (*path && *path != '/')
            path++;

        size_t len = (size_t)(path - start);

        /* Is this the final component? */
        const char *peek = path;
        while (*peek == '/')
            peek++;

        if (*peek == '\0' && parent_only) {
            *last     = start;
            *last_len = len;
            return node;
        }

        if (len == 1 && start[0] == '.')
            continue;

        if (len == 2 && start[0] == '.' && start[1] == '.') {
            if (node->parent)
                node = node->parent;
            continue;
        }

        struct fs_node *child = find_child(node, start, len);
        if (!child)
            return NULL;

        /* A path with more components can only continue through a
         * directory: "/file/x" is not a thing. */
        if (*peek != '\0' && child->type != FS_DIR)
            return NULL;

        node = child;
    }
}

struct fs_node *fs_lookup(struct fs_node *base, const char *path)
{
    if (!path || !*path)
        return NULL;

    return walk(base, path, false, NULL, NULL);
}

struct fs_node *fs_create(struct fs_node *base, const char *path,
                          enum fs_type type)
{
    const char *name = NULL;
    size_t      name_len = 0;

    if (!path || !*path)
        return NULL;

    struct fs_node *parent = walk(base, path, true, &name, &name_len);
    if (!parent || parent->type != FS_DIR)
        return NULL;

    if (name_len == 0 || name_len >= FS_NAME_MAX)
        return NULL;

    /* "." and ".." are not namable. */
    if ((name_len == 1 && name[0] == '.') ||
        (name_len == 2 && name[0] == '.' && name[1] == '.'))
        return NULL;

    if (find_child(parent, name, name_len))
        return NULL;    /* already exists */

    char buf[FS_NAME_MAX];
    memcpy(buf, name, name_len);
    buf[name_len] = '\0';

    struct fs_node *node = node_new(buf, type);
    if (!node)
        return NULL;

    node->parent = parent;

    /* Append to the end so directory listings keep creation order. */
    if (!parent->children) {
        parent->children = node;
    } else {
        struct fs_node *tail = parent->children;
        while (tail->sibling)
            tail = tail->sibling;
        tail->sibling = node;
    }

    return node;
}

/* Make sure a file can hold `needed` bytes. */
static bool ensure_capacity(struct fs_node *file, size_t needed)
{
    if (file->capacity >= needed)
        return true;

    /* Double until it fits, starting at a sensible minimum. Geometric
     * growth keeps a sequence of appends linear overall. */
    size_t capacity = file->capacity ? file->capacity : 64;
    while (capacity < needed)
        capacity *= 2;

    uint8_t *data = krealloc(file->data, capacity);
    if (!data)
        return false;

    file->data     = data;
    file->capacity = capacity;

    return true;
}

bool fs_write(struct fs_node *file, const void *data, size_t size)
{
    if (!file || file->type != FS_FILE)
        return false;

    if (!ensure_capacity(file, size))
        return false;

    memcpy(file->data, data, size);
    file->size = size;

    return true;
}

bool fs_append(struct fs_node *file, const void *data, size_t size)
{
    if (!file || file->type != FS_FILE)
        return false;

    if (!ensure_capacity(file, file->size + size))
        return false;

    memcpy(file->data + file->size, data, size);
    file->size += size;

    return true;
}

bool fs_unlink(struct fs_node *node)
{
    if (!node || node == root || !node->parent)
        return false;

    if (node->type == FS_DIR && node->children)
        return false;   /* refuse to orphan a subtree */

    struct fs_node *parent = node->parent;

    if (parent->children == node) {
        parent->children = node->sibling;
    } else {
        struct fs_node *prev = parent->children;
        while (prev && prev->sibling != node)
            prev = prev->sibling;
        if (!prev)
            return false;   /* not actually linked into its parent */
        prev->sibling = node->sibling;
    }

    kfree(node->data);
    kfree(node);
    node_count--;

    return true;
}

void fs_path(struct fs_node *node, char *buf, size_t size)
{
    if (!node || size == 0)
        return;

    if (node == root) {
        strlcpy(buf, "/", size);
        return;
    }

    /* Names are known from the leaf upwards, so collect them and then
     * emit in reverse. */
    struct fs_node *chain[32];
    int depth = 0;

    for (struct fs_node *n = node; n && n != root && depth < 32; n = n->parent)
        chain[depth++] = n;

    size_t pos = 0;
    for (int i = depth - 1; i >= 0; i--) {
        size_t name_len = strlen(chain[i]->name);

        if (pos + 1 + name_len + 1 > size)
            break;

        buf[pos++] = '/';
        memcpy(buf + pos, chain[i]->name, name_len);
        pos += name_len;
    }

    buf[pos] = '\0';
}

static size_t subtree_bytes(struct fs_node *node)
{
    size_t total = node->size;

    for (struct fs_node *child = node->children; child; child = child->sibling)
        total += subtree_bytes(child);

    return total;
}

size_t fs_total_bytes(void)
{
    return root ? subtree_bytes(root) : 0;
}

void fs_init(void)
{
    root = node_new("", FS_DIR);
    if (!root)
        panic("fs: could not allocate the root directory");

    /* A tiny starting tree, so there is something to look at. */
    fs_create(root, "/home", FS_DIR);
    fs_create(root, "/tmp", FS_DIR);
    fs_create(root, "/dev", FS_DIR);

    struct fs_node *readme = fs_create(root, "/home/readme.txt", FS_FILE);
    if (readme) {
        static const char text[] =
            "Nebula OS\n"
            "=========\n"
            "\n"
            "A 32-bit x86 kernel written from scratch: custom boot sector,\n"
            "protected mode, paging, a preemptive round-robin scheduler and\n"
            "this in-memory filesystem.\n"
            "\n"
            "Type `help` for the available commands.\n";
        fs_write(readme, text, sizeof(text) - 1);
    }

    struct fs_node *motd = fs_create(root, "/home/motd", FS_FILE);
    if (motd) {
        static const char text[] =
            "Everything here lives in RAM and is gone at reboot.\n";
        fs_write(motd, text, sizeof(text) - 1);
    }

    kprintf("[fs] in-memory filesystem mounted at / (%u nodes)\n", node_count);
}
