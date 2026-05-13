#include "ann.h"
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static float* g_data     = NULL;
static size_t g_count    = 0;
static int    g_dim      = 14;
static size_t g_map_size = 0;
static void*  g_map_base = NULL;
static int    g_map_owned = 0;

#if defined(__GNUC__) || defined(__clang__)
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x)   (x)
#define unlikely(x) (x)
#endif

#define ANN_STACK_CAP 256


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

static KDNode* g_nodes     = NULL;
static int     g_root      = -1;
static int     g_max_visits = 2046;


static inline float hsum256_ps(__m256 v) {
    __m128 lo  = _mm256_castps256_ps128(v);
    __m128 hi  = _mm256_extractf128_ps(v, 1);
    __m128 sum = _mm_add_ps(lo, hi);
    sum = _mm_hadd_ps(sum, sum);
    sum = _mm_hadd_ps(sum, sum);
    return _mm_cvtss_f32(sum);
}

static inline float distance_sq_14(const float* __restrict__ a, const float* __restrict__ b) {
    __m256 va0   = _mm256_load_ps(a);
    __m256 vb0   = _mm256_load_ps(b);
    __m256 diff0 = _mm256_sub_ps(va0, vb0);
    __m256 mul0  = _mm256_mul_ps(diff0, diff0);

    __m256 va1   = _mm256_load_ps(a + 8);
    __m256 vb1   = _mm256_load_ps(b + 8);
    __m256 diff1 = _mm256_sub_ps(va1, vb1);
    __m256 mul1  = _mm256_mul_ps(diff1, diff1);

    mul1 = _mm256_blend_ps(mul1, _mm256_setzero_ps(), 0xC0);

    return hsum256_ps(_mm256_add_ps(mul0, mul1));
}

static inline void push_candidate(
    float d, int idx, int k,
    int* indices, float* distances,
    int* worst, float* worst_val
) {
    if (d >= *worst_val) return;
    distances[*worst] = d;
    indices[*worst]   = idx;
    *worst     = 0;
    *worst_val = distances[0];
    for (int i = 1; i < k; i++) {
        if (distances[i] > *worst_val) {
            *worst_val = distances[i];
            *worst     = i;
        }
    }
}

static void kd_search(
    int node, const float* query, int k,
    int* indices, float* distances,
    int* worst, float* worst_val,
    int* visited, int max_visits
) {
    if (unlikely(node < 0) || unlikely(*visited >= max_visits)) return;

    int stack[ANN_STACK_CAP] __attribute__((aligned(64)));
    int top = 0;
    stack[top++] = node;

    while (likely(top > 0) && likely(*visited < max_visits)) {
        int cur = stack[--top];
        if (unlikely(cur < 0)) continue;

        (*visited)++;
        KDNode*      n   = &g_nodes[cur];
        const float* row = g_data + (size_t)n->idx * 16;
        __builtin_prefetch(row, 0, 3);

        uint32_t axis  = n->right_and_axis >> 28;
        uint32_t right = n->right_and_axis & 0x0FFFFFFF;
        int r = (right == 0x0FFFFFFF) ? -1 : (int)right;
        int l = (n->left == 0xFFFFFFFF) ? -1 : (int)n->left;

        float diff = query[axis] - n->split;
        int near = (diff <= 0.0f) ? l : r;
        int far  = (diff <= 0.0f) ? r : l;

        if (near >= 0) __builtin_prefetch(&g_nodes[near], 0, 3);
        if (far  >= 0) __builtin_prefetch(&g_nodes[far],  0, 3);

        float d = distance_sq_14(query, row);
        push_candidate(d, n->idx, k, indices, distances, worst, worst_val);

        if (far >= 0 && diff * diff < *worst_val) {
            if (likely(top < ANN_STACK_CAP)) stack[top++] = far;
        }
        if (near >= 0) {
            if (likely(top < ANN_STACK_CAP)) stack[top++] = near;
        }
    }
}


static int load_dataset(const char* path) {
    int fd = open(path, O_RDONLY);
    if (unlikely(fd < 0)) {
        fprintf(stderr, "[ann.c] erro abrindo dataset %s: %s\n", path, strerror(errno));
        return -errno;
    }

    struct stat st;
    if (unlikely(fstat(fd, &st) != 0)) {
        int err = -errno;
        close(fd);
        return err;
    }

    size_t size     = (size_t)st.st_size;
    size_t row_size = 64;
    if (unlikely(size == 0 || (size % row_size) != 0)) {
        fprintf(stderr, "[ann.c] tamanho inválido do dataset: %zu\n", size);
        close(fd);
        return -EINVAL;
    }

    void* map = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if (unlikely(map == MAP_FAILED)) return -errno;

    madvise(map, size, MADV_SEQUENTIAL);
    madvise(map, size, MADV_WILLNEED);

    g_map_size = size;
    if (((uintptr_t)map & 31u) == 0u) {
        g_map_base  = map;
        g_map_owned = 0;
        g_data      = (float*)map;
    } else {
        void* aligned = NULL;
        int err = posix_memalign(&aligned, 32, size);
        if (err != 0) { munmap(map, size); return -err; }
        memcpy(aligned, map, size);
        munmap(map, size);
        g_map_base  = aligned;
        g_map_owned = 1;
        g_data      = (float*)aligned;
    }

    g_count = size / row_size;
    return 0;
}


static int load_tree(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[ann.c] erro abrindo tree %s: %s\n", path, strerror(errno));
        return -errno;
    }

    TreeHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        fprintf(stderr, "[ann.c] erro lendo cabeçalho de %s\n", path);
        fclose(f);
        return -EIO;
    }

    if (hdr.magic != TREE_MAGIC) {
        fprintf(stderr, "[ann.c] magic inválido em %s — rode ann-build novamente\n", path);
        fclose(f);
        return -EINVAL;
    }

    if (hdr.version != TREE_VERSION) {
        fprintf(stderr, "[ann.c] versão incompatível %u (esperado %u)\n",
                hdr.version, TREE_VERSION);
        fclose(f);
        return -EINVAL;
    }

    if (hdr.count != (uint64_t)g_count) {
        fprintf(stderr, "[ann.c] tree.bin tem %llu nós mas dataset tem %zu pontos — "
                        "rode ann-build novamente\n",
                (unsigned long long)hdr.count, g_count);
        fclose(f);
        return -EINVAL;
    }

    g_nodes = (KDNode*)malloc(sizeof(KDNode) * g_count);
    if (!g_nodes) { fclose(f); return -ENOMEM; }

    if (fread(g_nodes, sizeof(KDNode), g_count, f) != g_count) {
        fprintf(stderr, "[ann.c] erro lendo nós de %s\n", path);
        free(g_nodes);
        g_nodes = NULL;
        fclose(f);
        return -EIO;
    }

    fclose(f);
    g_root = (int)hdr.root;
    fprintf(stderr, "[ann.c] tree carregada: %zu nós, root=%d\n", g_count, g_root);
    return 0;
}


ANN_API int init_dataset(const char* dataset_path) {
    if (g_data != NULL) return 0;

    const char* max_visits_env = getenv("ANN_MAX_VISITS");
    if (max_visits_env && *max_visits_env != '\0') {
        int v = atoi(max_visits_env);
        if (v > 0) g_max_visits = v;
    }

    int rc = load_dataset(dataset_path);
    if (rc != 0) return rc;

    if (g_count == 0) return 0;

    const char* tree_path = getenv("ANN_TREE_PATH");
    char tree_buf[4096];
    if (!tree_path) {
        const char* slash = strrchr(dataset_path, '/');
        if (slash) {
            size_t dir_len = (size_t)(slash - dataset_path + 1);
            snprintf(tree_buf, sizeof(tree_buf), "%.*stree.bin",
                     (int)dir_len, dataset_path);
        } else {
            snprintf(tree_buf, sizeof(tree_buf), "tree.bin");
        }
        tree_path = tree_buf;
    }

    rc = load_tree(tree_path);
    if (rc != 0) return rc;

    return 0;
}

ANN_API void free_dataset(void) {
    if (g_data != NULL) {
        if (g_map_owned) free(g_map_base);
        else             munmap(g_map_base, g_map_size);
        g_data      = NULL;
        g_count     = 0;
        g_map_size  = 0;
        g_map_base  = NULL;
        g_map_owned = 0;
    }
    if (g_nodes != NULL) {
        free(g_nodes);
        g_nodes = NULL;
        g_root  = -1;
    }
}

ANN_API size_t       dataset_count(void) { return g_count; }
ANN_API int          dataset_dim(void)   { return g_dim;   }

ANN_API unsigned int dataset_label(int index) {
    if (index < 0 || index >= (int)g_count) return 0;
    const char* row = (const char*)g_data + (size_t)index * 64;
    return (unsigned int)(*(uint64_t*)(row + 56));
}

ANN_API void ann_api(const float* query, int k, int* indices, float* distances) {
    if (k <= 0 || !query || !indices || !distances) return;

    for (int i = 0; i < k; i++) {
        distances[i] = FLT_MAX;
        indices[i]   = -1;
    }

    if (!g_data || g_count == 0) return;

    int   worst     = 0;
    float worst_val = distances[0];
    int   visited   = 0;

    if (likely(g_nodes != NULL && g_root >= 0)) {
        kd_search(g_root, query, k, indices, distances,
                  &worst, &worst_val, &visited, g_max_visits);
        return;
    }

    const int    dim  = g_dim;
    const size_t cnt  = g_count;
    const float* data = g_data;
    for (size_t i = 0; i < cnt; i++) {
        const float* row = data + i * 16;
        float d = distance_sq_14(query, row);
        push_candidate(d, (int)i, k, indices, distances, &worst, &worst_val);
    }
}


static void write_error(const char* msg) {
    fprintf(stderr, "[ann.c] error: %s\n", msg);
    exit(1);
}

int main(int argc, char** argv) {
    const char* dataset_path = "/app/resources/reference.bin";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dataset") == 0 && i + 1 < argc)
            dataset_path = argv[++i];
    }

    if (init_dataset(dataset_path) != 0) {
        write_error("dataset ou tree.bin — rode ann-build antes de iniciar");
        return 1;
    }

    printf("ready\n");
    fflush(stdout);

    const int K = 5;
    float        query[16] __attribute__((aligned(32)));
    query[14] = 0.0f;
    query[15] = 0.0f;

    int          indices[5];
    float        distances[5];
    unsigned int labels[5];

    while (1) {
        ssize_t bytes_read = 0;
        while (bytes_read < 56) {
            ssize_t r = read(STDIN_FILENO, ((char*)query) + bytes_read, 56 - bytes_read);
            if (r <= 0) goto exit_loop;
            bytes_read += r;
        }

        ann_api(query, K, indices, distances);

        for (int i = 0; i < 5; i++)
            labels[i] = indices[i] >= 0 ? dataset_label(indices[i]) : 0;

        char outbuf[60];
        memcpy(outbuf,      indices,   20);
        memcpy(outbuf + 20, distances, 20);
        memcpy(outbuf + 40, labels,    20);

        ssize_t written = 0;
        while (written < 60) {
            ssize_t w = write(STDOUT_FILENO, outbuf + written, 60 - written);
            if (w <= 0) goto exit_loop;
            written += w;
        }
    }

exit_loop:
    free_dataset();
    return 0;
}