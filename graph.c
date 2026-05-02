#include "graph.h"

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct IntPairNode {
    int a;
    int b;
    int count;
    struct IntPairNode *next;
} IntPairNode;

typedef struct IntPairHash {
    IntPairNode **buckets;
    size_t nbuckets;
    size_t count;
} IntPairHash;

typedef struct IdNode {
    int orig_id;
    int index;
    struct IdNode *next;
} IdNode;

typedef struct IdHash {
    IdNode **buckets;
    size_t nbuckets;
    size_t count;
} IdHash;

typedef struct DynIntArr {
    int *data;
    size_t len;
    size_t cap;
} DynIntArr;

struct Graph {
    IdHash vertices;
    DynIntArr *adj;
    size_t adj_cap;

    int n_vertices;
    long long n_edges_read;

    int loops;
    int multiple_edge_extras;

    IntPairHash edge_multiplicity;
};

static size_t hash_id(int id, size_t nbuckets) {
    unsigned x = (unsigned)id;
    return (size_t)(x * 2654435761u) % nbuckets;
}

static IdHash id_hash_new(size_t nbuckets) {
    IdHash h;

    h.nbuckets = nbuckets ? nbuckets : 16;
    h.buckets = (IdNode **)calloc(h.nbuckets, sizeof(IdNode *));
    h.count = 0;
    return h;
}

static void id_hash_free(IdHash *h) {
    size_t i;
    IdNode *n;
    IdNode *nx;

    if (!h || !h->buckets)
        return;
    for (i = 0; i < h->nbuckets; i++) {
        n = h->buckets[i];
        while (n) {
            nx = n->next;
            free(n);
            n = nx;
        }
    }
    free(h->buckets);
    h->buckets = NULL;
}

static int id_hash_resize(IdHash *h, size_t new_nbuckets) {
    IdNode **nb;
    size_t i;
    IdNode *n;
    IdNode *nx;
    size_t bi;

    nb = (IdNode **)calloc(new_nbuckets, sizeof(IdNode *));
    if (!nb)
        return -1;
    for (i = 0; i < h->nbuckets; i++) {
        n = h->buckets[i];
        while (n) {
            nx = n->next;
            bi = hash_id(n->orig_id, new_nbuckets);
            n->next = nb[bi];
            nb[bi] = n;
            n = nx;
        }
    }
    free(h->buckets);
    h->buckets = nb;
    h->nbuckets = new_nbuckets;
    return 0;
}

static int id_hash_get_or_insert(IdHash *h, int orig_id, int *index_out) {
    const size_t max_chain = 8;
    size_t bi;
    IdNode *n;
    int new_index;
    IdNode *node;

    if (h->count + 1 > h->nbuckets * max_chain / 10) {
        if (id_hash_resize(h, h->nbuckets * 2 + 1) != 0)
            return -1;
    }

    bi = hash_id(orig_id, h->nbuckets);
    for (n = h->buckets[bi]; n; n = n->next) {
        if (n->orig_id == orig_id) {
            *index_out = n->index;
            return 0;
        }
    }

    new_index = (int)h->count;
    node = (IdNode *)malloc(sizeof(IdNode));
    if (!node)
        return -1;
    node->orig_id = orig_id;
    node->index = new_index;
    node->next = h->buckets[bi];
    h->buckets[bi] = node;
    h->count++;
    *index_out = new_index;
    return 1;
}

static size_t hash_pair(int a, int b, size_t nbuckets) {
    unsigned ua = (unsigned)a;
    unsigned ub = (unsigned)b;
    unsigned long long mix = (unsigned long long)ua << 32 | ub;

    mix ^= mix >> 33;
    mix *= 0xff51afd7ed558ccdULL;
    mix ^= mix >> 33;
    return (size_t)(mix % nbuckets);
}

static IntPairHash pair_hash_new(size_t nbuckets) {
    IntPairHash h;

    h.nbuckets = nbuckets ? nbuckets : 16;
    h.buckets = (IntPairNode **)calloc(h.nbuckets, sizeof(IntPairNode *));
    h.count = 0;
    return h;
}

static void pair_hash_free(IntPairHash *h) {
    size_t i;
    IntPairNode *n;
    IntPairNode *nx;

    if (!h || !h->buckets)
        return;
    for (i = 0; i < h->nbuckets; i++) {
        n = h->buckets[i];
        while (n) {
            nx = n->next;
            free(n);
            n = nx;
        }
    }
    free(h->buckets);
    h->buckets = NULL;
}

static int pair_hash_resize(IntPairHash *h, size_t new_nbuckets) {
    IntPairNode **nb;
    size_t i;
    IntPairNode *n;
    IntPairNode *nx;
    size_t bi;

    nb = (IntPairNode **)calloc(new_nbuckets, sizeof(IntPairNode *));
    if (!nb)
        return -1;
    for (i = 0; i < h->nbuckets; i++) {
        n = h->buckets[i];
        while (n) {
            nx = n->next;
            bi = hash_pair(n->a, n->b, new_nbuckets);
            n->next = nb[bi];
            nb[bi] = n;
            n = nx;
        }
    }
    free(h->buckets);
    h->buckets = nb;
    h->nbuckets = new_nbuckets;
    return 0;
}

static void pair_note_edge(Graph *g, int iu, int iv) {
    int a;
    int b;
    IntPairHash *h;
    const size_t max_chain = 8;
    size_t bi;
    IntPairNode *n;
    IntPairNode *node;

    a = iu < iv ? iu : iv;
    b = iu < iv ? iv : iu;

    h = &g->edge_multiplicity;
    if (h->count + 1 > h->nbuckets * max_chain / 10) {
        pair_hash_resize(h, h->nbuckets * 2 + 1);
    }

    bi = hash_pair(a, b, h->nbuckets);
    for (n = h->buckets[bi]; n; n = n->next) {
        if (n->a == a && n->b == b) {
            n->count++;
            if (n->count > 1)
                g->multiple_edge_extras++;
            return;
        }
    }

    node = (IntPairNode *)malloc(sizeof(IntPairNode));
    if (!node)
        return;
    node->a = a;
    node->b = b;
    node->count = 1;
    node->next = h->buckets[bi];
    h->buckets[bi] = node;
    h->count++;
}

static int dyn_append(DynIntArr *a, int v) {
    size_t nc;
    int *nd;

    if (a->len >= a->cap) {
        nc = a->cap ? a->cap * 2 : 8;
        nd = (int *)realloc(a->data, nc * sizeof(int));
        if (!nd)
            return -1;
        a->data = nd;
        a->cap = nc;
    }
    a->data[a->len++] = v;
    return 0;
}

static void dyn_free(DynIntArr *a) {
    free(a->data);
    a->data = NULL;
    a->len = a->cap = 0;
}

static int ensure_adj(Graph *g, int min_vertices) {
    size_t nc;
    DynIntArr *na;
    size_t i;

    if ((size_t)min_vertices <= g->adj_cap)
        return 0;
    nc = g->adj_cap ? g->adj_cap : 8;
    while ((size_t)min_vertices > nc)
        nc *= 2;

    na = (DynIntArr *)realloc(g->adj, nc * sizeof(DynIntArr));
    if (!na)
        return -1;
    for (i = g->adj_cap; i < nc; i++) {
        na[i].data = NULL;
        na[i].len = 0;
        na[i].cap = 0;
    }
    g->adj = na;
    g->adj_cap = nc;
    return 0;
}

Graph *create_graph(int capacity) {
    Graph *g;
    size_t cap;

    g = (Graph *)calloc(1, sizeof(Graph));
    if (!g)
        return NULL;
    cap = (size_t)(capacity > 0 ? capacity : 16);
    g->vertices = id_hash_new(cap);
    g->edge_multiplicity = pair_hash_new(cap);
    g->adj = NULL;
    g->adj_cap = 0;
    g->n_vertices = 0;
    g->n_edges_read = 0;
    g->loops = 0;
    g->multiple_edge_extras = 0;
    return g;
}

void add_edge(Graph *g, int u, int v) {
    int iu;
    int ru;
    int iv;
    int nu;
    int nv;

    if (!g)
        return;

    g->n_edges_read++;

    if (u == v) {
        ru = id_hash_get_or_insert(&g->vertices, u, &iu);
        if (ru < 0)
            return;
        if (ru == 1) {
            g->n_vertices++;
            if (ensure_adj(g, g->n_vertices) != 0)
                return;
        }
        g->loops++;
        if (dyn_append(&g->adj[iu], iu) != 0)
            return;
        if (dyn_append(&g->adj[iu], iu) != 0)
            return;
        return;
    }

    nu = id_hash_get_or_insert(&g->vertices, u, &iu);
    if (nu < 0)
        return;
    nv = id_hash_get_or_insert(&g->vertices, v, &iv);
    if (nv < 0)
        return;
    if (nu == 1) {
        g->n_vertices++;
        if (ensure_adj(g, g->n_vertices) != 0)
            return;
    }
    if (nv == 1) {
        g->n_vertices++;
        if (ensure_adj(g, g->n_vertices) != 0)
            return;
    }

    pair_note_edge(g, iu, iv);

    if (dyn_append(&g->adj[iu], iv) != 0)
        return;
    if (dyn_append(&g->adj[iv], iu) != 0)
        return;
}

void free_graph(Graph *g) {
    size_t i;

    if (!g)
        return;
    id_hash_free(&g->vertices);
    pair_hash_free(&g->edge_multiplicity);
    if (g->adj) {
        for (i = 0; i < g->adj_cap; i++)
            dyn_free(&g->adj[i]);
        free(g->adj);
    }
    free(g);
}

int graph_num_vertices(const Graph *g) {
    return g ? g->n_vertices : 0;
}

long long graph_num_edge_records(const Graph *g) {
    return g ? g->n_edges_read : 0;
}

void compute_degrees(const Graph *g, DegreeStats *out) {
    int i;
    int d;

    if (!g || !out) {
        if (out)
            memset(out, 0, sizeof(*out));
        return;
    }

    memset(out, 0, sizeof(*out));

    if (g->n_vertices == 0) {
        out->degrees = NULL;
        out->min_degree = 0;
        out->max_degree = 0;
        return;
    }

    out->degrees = (int *)calloc((size_t)g->n_vertices, sizeof(int));
    if (!out->degrees)
        return;

    for (i = 0; i < g->n_vertices; i++)
        out->degrees[i] = (int)g->adj[i].len;

    out->min_degree = INT_MAX;
    out->max_degree = 0;
    for (i = 0; i < g->n_vertices; i++) {
        d = out->degrees[i];
        if (d < out->min_degree)
            out->min_degree = d;
        if (d > out->max_degree)
            out->max_degree = d;
    }
}

void free_degree_stats(DegreeStats *s) {
    if (!s)
        return;
    free(s->degrees);
    s->degrees = NULL;
}

void detect_multigraph(const Graph *g, MultigraphStats *out) {
    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    if (!g)
        return;

    out->loops = g->loops;
    out->multiple_edges = g->multiple_edge_extras;
    out->is_simple = (g->loops == 0 && g->multiple_edge_extras == 0);
}

static int cmp_int_asc(const void *pa, const void *pb) {
    int a = *(const int *)pa;
    int b = *(const int *)pb;

    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}

static int dfs_component_size(const Graph *g, int start_vertex,
                              unsigned char *visited) {
    size_t cap;
    int *stack;
    size_t sp;
    int discovered;
    int v;
    const DynIntArr *adj;
    size_t k;
    int w;
    size_t nc;
    int *ns;

    cap = (size_t)(g->n_vertices > 0 ? g->n_vertices : 8);
    stack = (int *)malloc(cap * sizeof(int));
    if (!stack)
        return -1;

    sp = 0;
    stack[sp++] = start_vertex;
    discovered = 0;

    while (sp > 0) {
        v = stack[--sp];
        if (visited[v])
            continue;
        visited[v] = 1;
        discovered++;

        adj = &g->adj[v];
        for (k = 0; k < adj->len; k++) {
            w = adj->data[k];
            if (!visited[w]) {
                if (sp >= cap) {
                    nc = cap * 2;
                    ns = (int *)realloc(stack, nc * sizeof(int));
                    if (!ns) {
                        free(stack);
                        return -1;
                    }
                    stack = ns;
                    cap = nc;
                }
                stack[sp++] = w;
            }
        }
    }

    free(stack);
    return discovered;
}

void dfs(const Graph *g, int start_vertex, unsigned char *visited) {
    (void)dfs_component_size(g, start_vertex, visited);
}

int count_components(const Graph *g, ComponentStats *out) {
    unsigned char *vis;
    int *sizes_buf;
    int ncomp;
    int i;
    int sz;

    if (!out)
        return 0;
    memset(out, 0, sizeof(*out));
    if (!g || g->n_vertices <= 0)
        return 0;

    vis = (unsigned char *)calloc((size_t)g->n_vertices, sizeof(unsigned char));
    if (!vis)
        return -1;

    sizes_buf = (int *)malloc((size_t)g->n_vertices * sizeof(int));
    if (!sizes_buf) {
        free(vis);
        return -1;
    }

    ncomp = 0;
    for (i = 0; i < g->n_vertices; i++) {
        if (vis[i])
            continue;

        sz = dfs_component_size(g, i, vis);
        if (sz < 0) {
            free(sizes_buf);
            free(vis);
            return -1;
        }
        sizes_buf[ncomp++] = sz;
    }

    qsort(sizes_buf, (size_t)ncomp, sizeof(int), cmp_int_asc);

    out->num_components = ncomp;
    out->sizes = sizes_buf;
    out->sizes_len = (size_t)ncomp;
    free(vis);
    return ncomp;
}

void free_component_stats(ComponentStats *s) {
    if (!s)
        return;
    free(s->sizes);
    s->sizes = NULL;
    s->sizes_len = 0;
    s->num_components = 0;
}
