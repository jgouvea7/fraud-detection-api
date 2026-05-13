#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>


typedef struct KDNode {
    float    split;
    uint32_t idx;
    uint32_t left;
    uint32_t right_and_axis;
} KDNode;


#define TREE_MAGIC   0x444E414E4E414B44ULL
#define TREE_VERSION 1

typedef struct TreeHeader {
    uint64_t magic;
    uint32_t version;
    uint32_t root;
    uint64_t count;
} TreeHeader;

static float*  g_data       = NULL;
static size_t  g_count      = 0;
static int     g_dim        = 14;
static KDNode* g_nodes      = NULL;
static int     g_next_node  = 0;
static int     g_build_axis = 0;

static int cmp_axis(const void* a, const void* b) {
    int ia = *(const int*)a;
    int ib = *(const int*)b;
    float va = g_data[(size_t)ia * 16 + (size_t)g_build_axis];
    float vb = g_data[(size_t)ib * 16 + (size_t)g_build_axis];
    return (va < vb) ? -1 : (va > vb) ? 1 : 0;
}

static int build_kdtree(int* idxs, int n, int depth) {
    if (n <= 0) return -1;

    int axis = depth % g_dim;
    g_build_axis = axis;
    qsort(idxs, (size_t)n, sizeof(int), cmp_axis);

    int mid  = n / 2;
    int node = g_next_node++;

    g_nodes[node].idx   = (uint32_t)idxs[mid];
    g_nodes[node].split = g_data[(size_t)idxs[mid] * 16 + (size_t)axis];

    uint32_t left_child  = (uint32_t)build_kdtree(idxs,           mid,           depth + 1);
    uint32_t right_child = (uint32_t)build_kdtree(idxs + mid + 1, n - mid - 1,   depth + 1);

    g_nodes[node].left            = left_child;
    g_nodes[node].right_and_axis  = (right_child & 0x0FFFFFFF) | ((uint32_t)axis << 28);

    return node;
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void die(const char* msg) {
    fprintf(stderr, "[ann-build] erro: %s\n", msg);
    exit(1);
}

static void die_errno(const char* msg) {
    fprintf(stderr, "[ann-build] erro: %s: %s\n", msg, strerror(errno));
    exit(1);
}

int main(int argc, char** argv) {
    const char* dataset_path = "/app/resources/reference.bin";
    const char* output_path  = "/app/resources/tree.bin";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dataset") == 0 && i + 1 < argc)
            dataset_path = argv[++i];
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
            output_path = argv[++i];
    }

    fprintf(stderr, "[ann-build] dataset : %s\n", dataset_path);
    fprintf(stderr, "[ann-build] output  : %s\n", output_path);

    double t0 = now_sec();

    int fd = open(dataset_path, O_RDONLY);
    if (fd < 0) die_errno("open dataset");

    struct stat st;
    if (fstat(fd, &st) != 0) die_errno("fstat dataset");

    size_t size     = (size_t)st.st_size;
    size_t row_size = 64;

    if (size == 0 || (size % row_size) != 0) {
        fprintf(stderr, "[ann-build] tamanho inválido: %zu bytes\n", size);
        die("dataset corrompido");
    }

    void* map = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if (map == MAP_FAILED) die_errno("mmap dataset");

    g_data  = (float*)map;
    g_count = size / row_size;

    fprintf(stderr, "[ann-build] %zu pontos carregados (%.1f MB) em %.2fs\n",
            g_count, size / 1e6, now_sec() - t0);

    g_nodes = (KDNode*)malloc(sizeof(KDNode) * g_count);
    if (!g_nodes) die("malloc nodes");

    int* idxs = (int*)malloc(sizeof(int) * g_count);
    if (!idxs) die("malloc idxs");

    for (size_t i = 0; i < g_count; i++) idxs[i] = (int)i;

    fprintf(stderr, "[ann-build] construindo KD-tree...\n");
    double t1   = now_sec();
    g_next_node = 0;
    int root    = build_kdtree(idxs, (int)g_count, 0);
    free(idxs);
    munmap(map, size);

    fprintf(stderr, "[ann-build] KD-tree pronta: %d nós, root=%d, em %.2fs\n",
            g_next_node, root, now_sec() - t1);

    fprintf(stderr, "[ann-build] gravando %s...\n", output_path);
    double t2 = now_sec();

    FILE* out = fopen(output_path, "wb");
    if (!out) die_errno("fopen output");

    TreeHeader hdr = {
        .magic   = TREE_MAGIC,
        .version = TREE_VERSION,
        .root    = (uint32_t)root,
        .count   = (uint64_t)g_count,
    };

    if (fwrite(&hdr, sizeof(hdr), 1, out) != 1)         die_errno("fwrite header");
    if (fwrite(g_nodes, sizeof(KDNode), g_count, out) != g_count) die_errno("fwrite nodes");

    fclose(out);
    free(g_nodes);

    double tree_mb = (sizeof(TreeHeader) + sizeof(KDNode) * g_count) / 1e6;
    fprintf(stderr, "[ann-build] gravado %.1f MB em %.2fs\n", tree_mb, now_sec() - t2);
    fprintf(stderr, "[ann-build] total: %.2fs — pronto!\n", now_sec() - t0);

    return 0;
}