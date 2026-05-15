#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "graph.h"

/* Auxiliar para identificar caracteres de espaço em branco (ASCII) */
static int is_ascii_space(unsigned char c) {
    return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' ||
           c == '\v';
}

/* Remove espaços em branco do início e fim da string de forma eficiente */
static void trim_inplace(char *s) {
    char *p = s;
    size_t n;

    while (*p && is_ascii_space((unsigned char)*p))
        p++;
    if (p != s)
        memmove(s, p, strlen(p) + 1);

    n = strlen(s);
    while (n > 0 && is_ascii_space((unsigned char)s[n - 1])) {
        s[n - 1] = '\0';
        n--;
    }
}

/**
 * Realiza o parsing de uma linha do arquivo.
 * Suporta formatos: "100 200", "100,200", "100 , 200", etc.
 */
static int parse_edge_line(char *line, int *u, int *v) {
    char *p;
    char *endptr;
    long lu, lv;

    trim_inplace(line);

    /* Ignora linhas vazias */
    if (line[0] == '\0')
        return -1;

    p = line;

    /* Extrai o primeiro vértice */
    errno = 0;
    lu = strtol(p, &endptr, 10);
    if (errno != 0 || endptr == p)
        return -2;
    p = endptr;

    /* Pula espaços e opcionalmente uma vírgula entre os números */
    while (*p && is_ascii_space((unsigned char)*p))
        p++;

    if (*p == ',') {
        p++;
        while (*p && is_ascii_space((unsigned char)*p))
            p++;
    } else if (*p == '\0') {
        return -2; /* Linha incompleta */
    }

    /* Extrai o segundo vértice */
    errno = 0;
    lv = strtol(p, &endptr, 10);
    if (errno != 0 || endptr == p)
        return -2;
    p = endptr;

    /* Valida se não há lixo após os números */
    while (*p && is_ascii_space((unsigned char)*p))
        p++;
    if (*p != '\0')
        return -2;

    /* Verifica limites do tipo int */
    if (lu < INT_MIN || lu > INT_MAX || lv < INT_MIN || lv > INT_MAX)
        return -5;

    *u = (int)lu;
    *v = (int)lv;
    return 0;
}

/**
 * Gera o relatório exigido pelos requisitos C, D e E da especificação.
 */
static void print_report(Graph *g) {
    DegreeStats ds;
    MultigraphStats ms;
    ComponentStats cs;
    size_t i;
    int bp;

    memset(&ds, 0, sizeof(ds));
    memset(&ms, 0, sizeof(ms));
    memset(&cs, 0, sizeof(cs));

    /* Executa algoritmos de análise definidos em graph.c */
    compute_degrees(g, &ds);
    detect_multigraph(g, &ms);
    
    if (count_components(g, &cs) < 0) {
        fprintf(stderr, "Erro: Memória insuficiente para calcular componentes.\n");
        free_degree_stats(&ds);
        return;
    }

    /* Requisito B/C: Resumo de estrutura */
    printf("--- RELATÓRIO ESTRUTURAL DO GRAFO ---\n");
    printf("Vértices identificados: %d\n", graph_num_vertices(g));
    printf("Arestas lidas (total): %lld\n", graph_num_edge_records(g));

    /* Requisito C: Graus */
    printf("\nGrau Mínimo: %d\n", ds.min_degree);
    printf("Grau Máximo: %d\n", ds.max_degree);

    /* Requisito D: Classificação */
    printf("\nTipo de Grafo: %s\n", ms.is_simple ? "Simples" : "Multigrafo");
    printf("Quantidade de Laços: %d\n", ms.loops);
    printf("Arestas Múltiplas (duplicatas): %d\n", ms.multiple_edges);

    /* Extra: Bipartição */
    bp = graph_is_bipartite(g);
    printf("Bipartido: %s\n", bp < 0 ? "Erro na verificação" : (bp ? "Sim" : "Não"));

    /* Requisito E: Componentes Conexos */
    printf("\nTotal de Componentes Conexos: %d\n", cs.num_components);
    printf("Distribuição de tamanhos: [");
    for (i = 0; i < cs.sizes_len; i++) {
        printf("%d%s", cs.sizes[i], (i + 1 < cs.sizes_len) ? ", " : "");
    }
    printf("]\n");

    /* Liberação de memória das estruturas de estatística */
    free_degree_stats(&ds);
    free_component_stats(&cs);
}

int main(int argc, char **argv) {
    FILE *fp;
    Graph *g;
    char buf[512], work[512];
    int line_no = 0, err = 0;
    int u, v, pr;

    /* Validação de argumento de linha de comando */
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <caminho_do_arquivo.csv>\n", argv[0]);
        return 1;
    }

    fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("Erro ao abrir arquivo");
        return 1;
    }

    /* Inicializa o grafo com capacidade inicial dinâmica */
    g = create_graph(16);
    if (!g) {
        fprintf(stderr, "Erro crítico: Falha na alocação do grafo.\n");
        fclose(fp);
        return 1;
    }

    /* Loop de carga primária (Requisito B) */
    while (fgets(buf, sizeof(buf), fp)) {
        line_no++;
        size_t len = strlen(buf);

        /* Verifica se a linha foi truncada pelo buffer */
        if (len > 0 && buf[len - 1] != '\n' && !feof(fp)) {
            fprintf(stderr, "Erro na linha %d: Tamanho excede o limite.\n", line_no);
            err = 1;
            break;
        }

        memcpy(work, buf, len + 1);
        pr = parse_edge_line(work, &u, &v);

        if (pr == -1) continue; /* Linha vazia ou comentário */
        
        if (pr != 0) {
            trim_inplace(work);
            fprintf(stderr, "Aviso (Linha %d): Formato inválido ignorado: \"%s\"\n", line_no, work);
            continue;
        }

        /* Adiciona aresta ao grafo */
        add_edge(g, u, v);
    }

    if (ferror(fp)) {
        perror("Erro durante a leitura");
        err = 1;
    }

    fclose(fp);

    if (!err) {
        /* Exportação opcional para visualização */
        FILE *dotf = fopen("graph_export.dot", "w");
        if (dotf) {
            graph_write_dot(g, dotf);
            fclose(dotf);
            fprintf(stderr, "Grafo exportado para 'graph_export.dot'.\n");
        }

        /* Gera o relatório final no stdout */
        print_report(g);
    }

    /* Liberação total de memória */
    free_graph(g);
    return err ? 1 : 0;
}