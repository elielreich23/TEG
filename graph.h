#ifndef GRAPH_H
#define GRAPH_H

#include <stddef.h>

typedef struct Graph Graph;

typedef struct DegreeStats {
    int *degrees;
    int min_degree;
    int max_degree;
} DegreeStats;

typedef struct MultigraphStats {
    int is_simple;
    int loops;
    int multiple_edges;
} MultigraphStats;

typedef struct ComponentStats {
    int num_components;
    int *sizes;
    size_t sizes_len;
} ComponentStats;

Graph *create_graph(int capacity);
void add_edge(Graph *g, int u, int v);
void free_graph(Graph *g);

int graph_num_vertices(const Graph *g);
long long graph_num_edge_records(const Graph *g);

void compute_degrees(const Graph *g, DegreeStats *out);
void free_degree_stats(DegreeStats *s);

void detect_multigraph(const Graph *g, MultigraphStats *out);

int count_components(const Graph *g, ComponentStats *out);
void free_component_stats(ComponentStats *s);

void dfs(const Graph *g, int start_vertex, unsigned char *visited);

#endif
