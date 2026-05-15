#ifndef GRAPH_H
#define GRAPH_H

#include <stddef.h>
#include <stdio.h>

/* * Tipo opaco para o Grafo. 
 * A implementação real (struct Graph) fica escondida no graph.c 
 */
typedef struct Graph Graph;

/* Estrutura para armazenar estatísticas de graus (Requisito C) */
typedef struct DegreeStats {
    int *degrees;    /* Vetor com o grau de cada vértice */
    int min_degree;  /* Menor grau encontrado no grafo */
    int max_degree;  /* Maior grau encontrado no grafo */
} DegreeStats;

/* Estrutura para classificação de multigrafos (Requisito D) */
typedef struct MultigraphStats {
    int is_simple;      /* 1 se for Simples, 0 se for Multigrafo */
    int loops;          /* Quantidade de laços (self-loops) */
    int multiple_edges; /* Quantidade de arestas múltiplas */
} MultigraphStats;

/* Estrutura para análise de componentes conexos (Requisito E) */
typedef struct ComponentStats {
    int num_components; /* Total de componentes conexos encontrados */
    int *sizes;         /* Vetor com o tamanho (nº de vértices) de cada componente */
    size_t sizes_len;   /* Quantidade de elementos no vetor de tamanhos */
} ComponentStats;

/* --- Funções de Ciclo de Vida --- */

/**
 * Cria e inicializa um novo grafo vazio.
 * @param capacity Sugestão de capacidade inicial de vértices.
 * @return Ponteiro para o grafo ou NULL em caso de falha de memória.
 */
Graph *create_graph(int capacity);

/**
 * Libera toda a memória alocada para o grafo e suas estruturas internas.
 */
void free_graph(Graph *g);

/* --- Funções de Manipulação e Acesso --- */

/**
 * Adiciona uma aresta não-direcionada entre u e v.
 * Lida internamente com a criação de novos vértices e detecção de duplicatas.
 */
void add_edge(Graph *g, int u, int v);

/**
 * Retorna o número total de vértices únicos identificados.
 */
int graph_num_vertices(const Graph *g);

/**
 * Retorna o número total de registros de arestas lidos do arquivo.
 */
long long graph_num_edge_records(const Graph *g);

/* --- Funções de Análise (Requisitos do PDF) --- */

/**
 * Calcula o grau de todos os vértices e identifica o máximo e o mínimo.
 * A memória de 'out->degrees' deve ser liberada com free_degree_stats.
 */
void compute_degrees(const Graph *g, DegreeStats *out);
void free_degree_stats(DegreeStats *s);

/**
 * Analisa se o grafo possui laços ou arestas múltiplas.
 */
void detect_multigraph(const Graph *g, MultigraphStats *out);

/**
 * Identifica os componentes conexos e seus respectivos tamanhos.
 * A memória de 'out->sizes' deve ser liberada com free_component_stats.
 * @return 0 em sucesso, -1 em erro de alocação.
 */
int count_components(const Graph *g, ComponentStats *out);
void free_component_stats(ComponentStats *s);

/* --- Funções Avançadas e Utilitárias --- */

/**
 * Verifica se o grafo é bipartido (coloração com 2 cores).
 * @return 1 se sim, 0 se não, -1 em erro de memória.
 */
int graph_is_bipartite(const Graph *g);

/**
 * Realiza uma busca em profundidade (DFS) a partir de um vértice.
 * @param visited Vetor de booleanos para controle de visitação.
 */
void dfs(const Graph *g, int start_vertex, unsigned char *visited);

/**
 * Exporta a estrutura atual do grafo para o formato DOT (Graphviz).
 * @param fp Ponteiro de arquivo aberto para escrita.
 */
void graph_write_dot(const Graph *g, FILE *fp);

#endif /* GRAPH_H */