#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "graph.h"

static int is_ascii_space(unsigned char c) {
    return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' ||
           c == '\v';
}

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
static int parse_edge_line(char *line, int *u, int *v) {
    char *p;
    char *endptr;
    long lu;
    long lv;

    trim_inplace(line);

    /* linha vazia */
    if (line[0] == '\0')
        return -1;

    /*
       Aceita:
       100,200
       100 200
       100 , 200
       100\t200
       100, 200
    */

    p = line;

    errno = 0;
    lu = strtol(p, &endptr, 10);
    if (errno != 0 || endptr == p)
        return -2;
    p = endptr;

    while (*p && is_ascii_space((unsigned char)*p))
        p++;

    if (*p == ',') {
        p++;
        while (*p && is_ascii_space((unsigned char)*p))
            p++;
    } else {
        if (*p == '\0')
            return -2;
    }

    errno = 0;
    lv = strtol(p, &endptr, 10);
    if (errno != 0 || endptr == p)
        return -2;
    p = endptr;

    while (*p && is_ascii_space((unsigned char)*p))
        p++;
    if (*p != '\0')
        return -2;

    if (lu < INT_MIN || lu > INT_MAX || lv < INT_MIN || lv > INT_MAX)
        return -5;

    *u = (int)lu;
    *v = (int)lv;
    return 0;
}

/*static int parse_edge_line(char *line, int *u, int *v) {
    char *comma;
    char *rest;
    char *endptr;
    long lu;
    long lv;

    trim_inplace(line);
    if (line[0] == '\0')
        return -1;

    comma = strchr(line, ',');
    if (!comma)
        return -2;

    *comma = '\0';
    rest = comma + 1;

    trim_inplace(line);
    trim_inplace(rest);

    endptr = NULL;
    errno = 0;
    lu = strtol(line, &endptr, 10);
    if (errno != 0 || endptr == line || *endptr != '\0')
        return -3;

    endptr = NULL;
    errno = 0;
    lv = strtol(rest, &endptr, 10);
    if (errno != 0 || endptr == rest || *endptr != '\0')
        return -4;

    if (lu < INT_MIN || lu > INT_MAX || lv < INT_MIN || lv > INT_MAX)
        return -5;

    *u = (int)lu;
    *v = (int)lv;
    return 0;
}*/

static void print_report(Graph *g) {
    DegreeStats ds;
    MultigraphStats ms;
    ComponentStats cs;
    size_t i;
    int bp;

    memset(&ds, 0, sizeof(ds));
    memset(&ms, 0, sizeof(ms));
    memset(&cs, 0, sizeof(cs));

    compute_degrees(g, &ds);
    detect_multigraph(g, &ms);
    if (count_components(g, &cs) < 0) {
        fputs("error: out of memory computing components\n", stderr);
        free_degree_stats(&ds);
        free_component_stats(&cs);
        return;
    }

    printf("Vertices: %d\n", graph_num_vertices(g));
    printf("Edges: %lld\n", (long long)graph_num_edge_records(g));

    printf("\nMin degree: %d\n", ds.min_degree);
    printf("Max degree: %d\n", ds.max_degree);

    printf("\nGraph type: %s\n", ms.is_simple ? "Simple" : "Multigraph");
    printf("Loops: %d\n", ms.loops);
    printf("Multiple edges: %d\n", ms.multiple_edges);

    bp = graph_is_bipartite(g);
    printf("Bipartite: %s\n",
           bp < 0 ? "unknown (error)" : (bp ? "yes" : "no"));

    printf("\nConnected components: %d\n", cs.num_components);
    printf("Component sizes: [");
    for (i = 0; i < cs.sizes_len; i++) {
        printf("%d%s", cs.sizes[i],
               (i + 1 < cs.sizes_len) ? ", " : "");
    }
    printf("]\n");

    free_degree_stats(&ds);
    free_component_stats(&cs);
}

int main(int argc, char **argv) {
    FILE *fp;
    Graph *g;
    char buf[512];
    int line_no;
    int err;
    size_t len;
    char work[512];
    int u;
    int v;
    int pr;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <edges.csv>\n", argv[0]);
        return 1;
    }

    fp = fopen(argv[1], "rb");
    if (!fp) {
        perror(argv[1]);
        return 1;
    }

    g = create_graph(16);
    if (!g) {
        fputs("error: create_graph failed\n", stderr);
        fclose(fp);
        return 1;
    }

    line_no = 0;
    err = 0;

    while (fgets(buf, sizeof(buf), fp)) {
        line_no++;
        len = strlen(buf);
        if (len > 0 && buf[len - 1] != '\n' && !feof(fp)) {
            fprintf(stderr, "%s:%d: line too long\n", argv[1], line_no);
            err = 1;
            break;
        }

        if (len >= sizeof(work)) {
            fprintf(stderr, "%s:%d: line too long\n", argv[1], line_no);
            err = 1;
            break;
        }
        memcpy(work, buf, len + 1);

        pr = parse_edge_line(work, &u, &v);
        if (pr == -1)
            continue;
        if (pr != 0) {
            trim_inplace(work);
            fprintf(stderr, "%s:%d: malformed edge skipped: \"%s\"\n",
                    argv[1], line_no, work);
            continue;
        }

        add_edge(g, u, v);
    }

    if (ferror(fp)) {
        perror(argv[1]);
        err = 1;
    }

    fclose(fp);

    if (!err)
        print_report(g);

    free_graph(g);
    return err ? 1 : 0;
}
