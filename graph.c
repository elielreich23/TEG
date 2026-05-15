#include "graph.h"
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Estruturas Internas (Privadas) --- */

/* Nó para a tabela hash de pares de inteiros (identifica arestas múltiplas) */
typedef struct IntPairNode {
    int a, b;
    int count;
    struct IntPairNode *next;
} IntPairNode;

typedef struct IntPairHash {
    IntPairNode **buckets;
    size_t nbuckets;
    size_t count;
} IntPairHash;

/* Nó para mapear IDs originais do arquivo para índices internos (0 a N-1) */
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

/* Vetor dinâmico de inteiros para a lista de adjacência */
typedef struct DynIntArr {
    int *data;
    size_t len;
    size_t cap;
} DynIntArr;

/* Definição da estrutura principal do Grafo */
struct Graph {
    IdHash vertices;           /* Tabela hash de IDs -> Índices */
    DynIntArr *adj;            /* Array de listas de adjacência */
    size_t adj_cap;            /* Capacidade alocada para o array adj */
    int n_vertices;            /* Contador de vértices únicos */
    long long n_edges_read;    /* Total de arestas processadas */
    int loops;                 /* Contador de auto-laços (v, v) */
    int multiple_edge_extras;  /* Contador de ocorrências extras de uma mesma aresta */
    IntPairHash edge_multiplicity; /* Tabela hash para contar repetições de arestas */
};

/* --- Funções Auxiliares de Memória e Hash --- */

static size_t hash_id(int id, size_t nbuckets) {
    unsigned x = (unsigned)id;
    return (size_t)(x * 2654435761u) % nbuckets;
}

static size_t hash_pair(int a, int b, size_t nbuckets) {
    unsigned ua = (unsigned)a;
    unsigned ub = (unsigned)b;
    if (ua > ub) { unsigned tmp = ua; ua = ub; ub = tmp; }
    return (size_t)(ua * 31 + ub) % nbuckets;
}

/* --- Implementação da API (graph.h) --- */

Graph *create_graph(int capacity) {
    if (capacity <= 0) capacity = 16;
    Graph *g = (Graph *)malloc(sizeof(Graph));
    if (!g) return NULL;

    g->n_vertices = 0;
    g->n_edges_read = 0;
    g->loops = 0;
    g->multiple_edge_extras = 0;

    /* Inicializa Hash de Vértices */
    g->vertices.nbuckets = (size_t)capacity;
    g->vertices.buckets = (IdNode **)calloc(g->vertices.nbuckets, sizeof(IdNode *));
    g->vertices.count = 0;

    /* Inicializa Listas de Adjacência */
    g->adj_cap = (size_t)capacity;
    g->adj = (DynIntArr *)calloc(g->adj_cap, sizeof(DynIntArr));

    /* Inicializa Hash de Arestas Múltiplas */
    g->edge_multiplicity.nbuckets = (size_t)capacity;
    g->edge_multiplicity.buckets = (IntPairNode **)calloc(g->edge_multiplicity.nbuckets, sizeof(IntPairNode *));
    g->edge_multiplicity.count = 0;

    return g;
}

/* Mapeia um ID original para um índice interno sequencial */
static int get_or_create_index(Graph *g, int orig_id) {
    size_t h = hash_id(orig_id, g->vertices.nbuckets);
    IdNode *node = g->vertices.buckets[h];
    while (node) {
        if (node->orig_id == orig_id) return node->index;
        node = node->next;
    }

    /* Cria novo mapeamento se não existir */
    int new_idx = g->n_vertices++;
    IdNode *new_node = (IdNode *)malloc(sizeof(IdNode));
    new_node->orig_id = orig_id;
    new_node->index = new_idx;
    new_node->next = g->vertices.buckets[h];
    g->vertices.buckets[h] = new_node;
    g->vertices.count++;

    /* Redimensiona o array de adjacências se necessário */
    if ((size_t)new_idx >= g->adj_cap) {
        size_t new_cap = g->adj_cap * 2;
        g->adj = (DynIntArr *)realloc(g->adj, new_cap * sizeof(DynIntArr));
        memset(g->adj + g->adj_cap, 0, (new_cap - g->adj_cap) * sizeof(DynIntArr));
        g->adj_cap = new_cap;
    }
    return new_idx;
}

void add_edge(Graph *g, int u_id, int v_id) {
    int u = get_or_create_index(g, u_id);
    int v = get_or_create_index(g, v_id);
    g->n_edges_read++;

    /* Verifica Laços (Requisito D) */
    if (u == v) {
        g->loops++;
    }

    /* Verifica Arestas Múltiplas (Requisito D) utilizando Hash de pares */
    size_t h = hash_pair(u, v, g->edge_multiplicity.nbuckets);
    IntPairNode *node = g->edge_multiplicity.buckets[h];
    int found = 0;
    while (node) {
        if ((node->a == u && node->b == v) || (node->a == v && node->b == u)) {
            node->count++;
            g->multiple_edge_extras++;
            found = 1;
            break;
        }
        node = node->next;
    }
    if (!found) {
        IntPairNode *new_n = (IntPairNode *)malloc(sizeof(IntPairNode));
        new_n->a = u; new_n->b = v; new_n->count = 1;
        new_n->next = g->edge_multiplicity.buckets[h];
        g->edge_multiplicity.buckets[h] = new_n;
        
        /* Adiciona na lista de adjacência (apenas se for a primeira vez da aresta) */
        for (int i = 0; i < 2; i++) {
            int src = (i == 0) ? u : v;
            int dst = (i == 0) ? v : u;
            if (u == v && i == 1) break; /* Evita duplicar laço na lista */

            DynIntArr *a = &g->adj[src];
            if (a->len >= a->cap) {
                a->cap = a->cap ? a->cap * 2 : 4;
                a->data = (int *)realloc(a->data, a->cap * sizeof(int));
            }
            a->data[a->len++] = dst;
        }
    }
}

/* --- Cálculos e Estatísticas --- */

void compute_degrees(const Graph *g, DegreeStats *out) {
    out->degrees = (int *)malloc((size_t)g->n_vertices * sizeof(int));
    out->min_degree = (g->n_vertices > 0) ? INT_MAX : 0;
    out->max_degree = 0;

    for (int i = 0; i < g->n_vertices; i++) {
        int d = (int)g->adj[i].len;
        out->degrees[i] = d;
        if (d < out->min_degree) out->min_degree = d;
        if (d > out->max_degree) out->max_degree = d;
    }
}

void detect_multigraph(const Graph *g, MultigraphStats *out) {
    out->loops = g->loops;
    out->multiple_edges = g->multiple_edge_extras;
    out->is_simple = (g->loops == 0 && g->multiple_edge_extras == 0);
}

static int compare_ints(const void *a, const void *b) {
    return (*(int *)a - *(int *)b);
}

int count_components(const Graph *g, ComponentStats *out) {
    if (g->n_vertices == 0) {
        out->num_components = 0;
        out->sizes = NULL;
        out->sizes_len = 0;
        return 0;
    }

    unsigned char *visited = (unsigned char *)calloc((size_t)g->n_vertices, 1);
    int *comp_sizes = (int *)malloc((size_t)g->n_vertices * sizeof(int));
    int n_comp = 0;

    /* DFS Iterativa para evitar estouro de pilha em grafos grandes */
    int *stack = (int *)malloc((size_t)g->n_vertices * sizeof(int));

    for (int i = 0; i < g->n_vertices; i++) {
        if (!visited[i]) {
            int size = 0;
            int top = 0;
            stack[top++] = i;
            visited[i] = 1;

            while (top > 0) {
                int curr = stack[--top];
                size++;
                for (size_t k = 0; k < g->adj[curr].len; k++) {
                    int neighbor = g->adj[curr].data[k];
                    if (!visited[neighbor]) {
                        visited[neighbor] = 1;
                        stack[top++] = neighbor;
                    }
                }
            }
            comp_sizes[n_comp++] = size;
        }
    }

    /* Ordena tamanhos conforme requisito E */
    qsort(comp_sizes, (size_t)n_comp, sizeof(int), compare_ints);

    out->num_components = n_comp;
    out->sizes = comp_sizes;
    out->sizes_len = (size_t)n_comp;

    free(visited);
    free(stack);
    return 0;
}

/* --- Limpeza de Memória --- */

void free_degree_stats(DegreeStats *s) { if (s) free(s->degrees); }
void free_component_stats(ComponentStats *s) { if (s) free(s->sizes); }

void free_graph(Graph *g) {
    if (!g) return;
    for (size_t i = 0; i < g->vertices.nbuckets; i++) {
        IdNode *n = g->vertices.buckets[i];
        while (n) { IdNode *t = n; n = n->next; free(t); }
    }
    free(g->vertices.buckets);
    for (size_t i = 0; i < g->adj_cap; i++) free(g->adj[i].data);
    free(g->adj);
    for (size_t i = 0; i < g->edge_multiplicity.nbuckets; i++) {
        IntPairNode *n = g->edge_multiplicity.buckets[i];
        while (n) { IntPairNode *t = n; n = n->next; free(t); }
    }
    free(g->edge_multiplicity.buckets);
    free(g);
}

/* Getters simples */
int graph_num_vertices(const Graph *g) { return g->n_vertices; }
long long graph_num_edge_records(const Graph *g) { return g->n_edges_read; }

/**
 * Verifica se o grafo é bipartido usando o algoritmo de coloração (BFS).
 * Retorna 1 para Sim, 0 para Não e -1 em erro de alocação.
 */
int graph_is_bipartite(const Graph *g) {
    if (!g || g->n_vertices == 0) return 1;
    if (g->loops > 0) return 0; /* Laços impedem bipartição */

    int n = g->n_vertices;
    int *color = (int *)malloc((size_t)n * sizeof(int));
    int *queue = (int *)malloc((size_t)n * sizeof(int));
    
    if (!color || !queue) {
        free(color); free(queue);
        return -1;
    }

    /* Inicializa todas as cores como -1 (não colorido) */
    for (int i = 0; i < n; i++) color[i] = -1;

    for (int i = 0; i < n; i++) {
        if (color[i] == -1) {
            int head = 0, tail = 0;
            color[i] = 0;
            queue[tail++] = i;

            while (head < tail) {
                int v = queue[head++];
                for (size_t k = 0; k < g->adj[v].len; k++) {
                    int neighbor = g->adj[v].data[k];
                    if (color[neighbor] == -1) {
                        color[neighbor] = 1 - color[v];
                        queue[tail++] = neighbor;
                    } else if (color[neighbor] == color[v]) {
                        free(color); free(queue);
                        return 0; /* Conflito de cor: não é bipartido */
                    }
                }
            }
        }
    }

    free(color); free(queue);
    return 1;
}

/* Exportação DOT para visualização */
void graph_write_dot(const Graph *g, FILE *fp) {
    fprintf(fp, "graph G {\n  node [shape=circle];\n");
    /* Aqui poderíamos mapear de volta para os IDs originais se necessário */
    for (int i = 0; i < g->n_vertices; i++) {
        for (size_t k = 0; k < g->adj[i].len; k++) {
            int j = g->adj[i].data[k];
            if (i <= j) fprintf(fp, "  %d -- %d;\n", i, j);
        }
    }
    fprintf(fp, "}\n");
}