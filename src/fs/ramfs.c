#include <stdint.h>
#include "mvh/fs.h"

#define FS_NODE_MAX 64u

typedef struct {
    uint8_t used;
    uint8_t is_directory;
    uint8_t parent;
    uint16_t size;
    char name[FS_NAME_MAX];
    char data[FS_DATA_MAX];
} fs_node_t;

static fs_node_t nodes[FS_NODE_MAX];
static uint8_t current_directory;

static uint32_t string_length(const char *text)
{
    uint32_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static void string_copy(char *target, const char *source, uint32_t capacity)
{
    uint32_t index = 0;
    if (capacity == 0u) {
        return;
    }
    while (source[index] != '\0' && index + 1u < capacity) {
        target[index] = source[index];
        index++;
    }
    target[index] = '\0';
}

static int string_equal(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (*left++ != *right++) {
            return 0;
        }
    }
    return *left == *right;
}

static int find_child(uint8_t parent, const char *name)
{
    uint32_t index;
    for (index = 1; index < FS_NODE_MAX; index++) {
        if (nodes[index].used != 0u && nodes[index].parent == parent &&
            string_equal(nodes[index].name, name)) {
            return (int)index;
        }
    }
    return -1;
}

static int resolve(const char *path)
{
    uint8_t node = path[0] == '/' ? 0u : current_directory;
    char segment[FS_NAME_MAX];
    uint32_t position = path[0] == '/' ? 1u : 0u;
    uint32_t length;
    int child;
    while (path[position] != '\0') {
        while (path[position] == '/') {
            position++;
        }
        if (path[position] == '\0') {
            break;
        }
        length = 0;
        while (path[position] != '\0' && path[position] != '/') {
            if (length + 1u >= FS_NAME_MAX) {
                return -1;
            }
            segment[length++] = path[position++];
        }
        segment[length] = '\0';
        if (string_equal(segment, ".")) {
            continue;
        }
        if (string_equal(segment, "..")) {
            node = nodes[node].parent;
            continue;
        }
        if (nodes[node].is_directory == 0u) {
            return -1;
        }
        child = find_child(node, segment);
        if (child < 0) {
            return -1;
        }
        node = (uint8_t)child;
    }
    return (int)node;
}

static int resolve_parent(const char *path, uint8_t *parent, char *name)
{
    char parent_path[FS_PATH_MAX];
    uint32_t length = string_length(path);
    uint32_t split = length;
    uint32_t index;
    int resolved;
    if (length == 0u || length >= FS_PATH_MAX || path[length - 1u] == '/') {
        return -1;
    }
    while (split > 0u && path[split - 1u] != '/') {
        split--;
    }
    if (length - split == 0u || length - split >= FS_NAME_MAX) {
        return -1;
    }
    for (index = 0; index < length - split; index++) {
        name[index] = path[split + index];
    }
    name[length - split] = '\0';
    if (string_equal(name, ".") || string_equal(name, "..")) {
        return -1;
    }
    if (split == 0u) {
        *parent = current_directory;
        return 0;
    }
    if (split == 1u) {
        parent_path[0] = '/';
        parent_path[1] = '\0';
    } else {
        for (index = 0; index < split - 1u; index++) {
            parent_path[index] = path[index];
        }
        parent_path[split - 1u] = '\0';
    }
    resolved = resolve(parent_path);
    if (resolved < 0 || nodes[resolved].is_directory == 0u) {
        return -1;
    }
    *parent = (uint8_t)resolved;
    return 0;
}

static int create_node(const char *path, uint8_t is_directory)
{
    uint8_t parent;
    char name[FS_NAME_MAX];
    uint32_t index;
    if (resolve_parent(path, &parent, name) != 0 || find_child(parent, name) >= 0) {
        return -1;
    }
    for (index = 1; index < FS_NODE_MAX; index++) {
        if (nodes[index].used == 0u) {
            nodes[index].used = 1u;
            nodes[index].is_directory = is_directory;
            nodes[index].parent = parent;
            nodes[index].size = 0u;
            nodes[index].data[0] = '\0';
            string_copy(nodes[index].name, name, FS_NAME_MAX);
            return 0;
        }
    }
    return -1;
}

void fs_init(void)
{
    uint32_t index;
    for (index = 0; index < FS_NODE_MAX; index++) {
        nodes[index].used = 0u;
    }
    nodes[0].used = 1u;
    nodes[0].is_directory = 1u;
    nodes[0].parent = 0u;
    nodes[0].name[0] = '\0';
    current_directory = 0u;
    fs_mkdir("/home");
    fs_mkdir("/bin");
    fs_mkdir("/dev");
    fs_mkdir("/etc");
    fs_mkdir("/kernel");
    fs_mkdir("/tmp");
    fs_mkdir("/var");
    fs_touch("/home/welcome.txt");
    fs_write("/home/welcome.txt", "Welcome to MVH Kernel 1.1\n", 0u);
    fs_touch("/etc/version");
    fs_write("/etc/version", "1.1\n", 0u);
    fs_touch("/kernel/release");
    fs_write("/kernel/release", "MVHKernel 1.1 x86_64\n", 0u);
}

int fs_chdir(const char *path)
{
    int node = resolve(path);
    if (node < 0 || nodes[node].is_directory == 0u) {
        return -1;
    }
    current_directory = (uint8_t)node;
    return 0;
}

int fs_pwd(char *output, uint32_t capacity)
{
    uint8_t chain[FS_NODE_MAX];
    uint8_t node = current_directory;
    uint32_t depth = 0;
    uint32_t position = 0;
    uint32_t index;
    uint32_t length;
    if (capacity < 2u) {
        return -1;
    }
    while (node != 0u && depth < FS_NODE_MAX) {
        chain[depth++] = node;
        node = nodes[node].parent;
    }
    output[position++] = '/';
    while (depth > 0u) {
        node = chain[--depth];
        length = string_length(nodes[node].name);
        if (position + length + 1u >= capacity) {
            return -1;
        }
        for (index = 0; index < length; index++) {
            output[position++] = nodes[node].name[index];
        }
        if (depth > 0u) {
            output[position++] = '/';
        }
    }
    output[position] = '\0';
    return 0;
}

int fs_list(const char *path, fs_entry_t *entries, uint32_t capacity)
{
    int node = path[0] == '\0' ? (int)current_directory : resolve(path);
    uint32_t index;
    uint32_t count = 0;
    if (node < 0) {
        return -1;
    }
    if (nodes[node].is_directory == 0u) {
        if (capacity == 0u) {
            return 0;
        }
        string_copy(entries[0].name, nodes[node].name, FS_NAME_MAX);
        entries[0].is_directory = 0u;
        entries[0].size = nodes[node].size;
        return 1;
    }
    for (index = 1; index < FS_NODE_MAX && count < capacity; index++) {
        if (nodes[index].used != 0u && nodes[index].parent == (uint8_t)node) {
            string_copy(entries[count].name, nodes[index].name, FS_NAME_MAX);
            entries[count].is_directory = nodes[index].is_directory;
            entries[count].size = nodes[index].size;
            count++;
        }
    }
    return (int)count;
}

int fs_mkdir(const char *path)
{
    return create_node(path, 1u);
}

int fs_touch(const char *path)
{
    int node = resolve(path);
    if (node >= 0) {
        return nodes[node].is_directory == 0u ? 0 : -1;
    }
    return create_node(path, 0u);
}

int fs_write(const char *path, const char *text, uint8_t append)
{
    int node = resolve(path);
    uint32_t position;
    uint32_t index = 0;
    if (node < 0 || nodes[node].is_directory != 0u) {
        return -1;
    }
    position = append != 0u ? nodes[node].size : 0u;
    while (text[index] != '\0' && position + 1u < FS_DATA_MAX) {
        nodes[node].data[position++] = text[index++];
    }
    nodes[node].data[position] = '\0';
    nodes[node].size = (uint16_t)position;
    return text[index] == '\0' ? 0 : -2;
}

int fs_read(const char *path, const char **data, uint16_t *size)
{
    int node = resolve(path);
    if (node < 0 || nodes[node].is_directory != 0u) {
        return -1;
    }
    *data = nodes[node].data;
    *size = nodes[node].size;
    return 0;
}

int fs_remove(const char *path)
{
    int node = resolve(path);
    uint32_t index;
    if (node <= 0) {
        return -1;
    }
    if (nodes[node].is_directory != 0u) {
        for (index = 1; index < FS_NODE_MAX; index++) {
            if (nodes[index].used != 0u && nodes[index].parent == (uint8_t)node) {
                return -2;
            }
        }
    }
    nodes[node].used = 0u;
    return 0;
}
