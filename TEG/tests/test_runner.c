#include "../graph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

static void fail(const char *msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
    failures++;
}

static void assert_int(int got, int exp, const char *what) {
    if (got != exp) {
        fprintf(stderr, "%s: expected %d, got %d\n", what, exp, got);
        failures++;
    }
}

static void assert_ll(long long got, long long exp, const char *what) {
    if (got != exp) {
        fprintf(stderr, "%s: expected %lld, got %lld\n", what, exp, got);
        failures++;
    }
}

static int cmp_int(const void *a, const void *b) {
    int x = *(const int *)a;
    int y = *(const int *)b;
    return (x > y) - (x < y);
}

static void assert_sizes(int *got, int ng, int *exp, int ne, const char *what) {
    if (ng != ne) {
        fprintf(stderr, "%s: size count %d vs %d\n", what, ng, ne);
        failures++;
        return;
    }
    int *gc = (int *)malloc((size_t)ng * sizeof(int));
    int *ec = (int *)malloc((size_t)ne * sizeof(int));
    if (!gc || !ec) {
        free(gc);
        free(ec);
        fail("OOM in assert_sizes");
        return;
    }
    memcpy(gc, got, (size_t)ng * sizeof(int));
    memcpy(ec, exp, (size_t)ne * sizeof(int));
    qsort(gc, (size_t)ng, sizeof(int), cmp_int);
    qsort(ec, (size_t)ne, sizeof(int), cmp_int);
    for (int i = 0; i < ng; i++) {
        if (gc[i] != ec[i]) {
            fprintf(stderr, "%s: component sizes mismatch at %d\n", what, i);
            failures++;
            break;
        }
    }
    free(gc);
    free(ec);
}

static void test_case_1_simple_chain(void) {
    Graph *g = create_graph(8);
    add_edge(g, 1, 2);
    add_edge(g, 2, 3);

    DegreeStats ds = {0};
    compute_degrees(g, &ds);
    assert_int(ds.min_degree, 1, "case1 min degree");
    assert_int(ds.max_degree, 2, "case1 max degree");
    free_degree_stats(&ds);

    MultigraphStats ms = {0};
    detect_multigraph(g, &ms);
    assert_int(ms.is_simple, 1, "case1 simple flag");
    assert_int(ms.loops, 0, "case1 loops");
    assert_int(ms.multiple_edges, 0, "case1 multi");

    ComponentStats cs = {0};
    count_components(g, &cs);
    assert_int(cs.num_components, 1, "case1 components");
    int ex[] = {3};
    assert_sizes(cs.sizes, cs.num_components, ex, 1, "case1 sizes");
    free_component_stats(&cs);

    assert_int(graph_num_vertices(g), 3, "case1 |V|");
    assert_ll(graph_num_edge_records(g), 2, "case1 records");

    free_graph(g);
}

static void test_case_2_loop(void) {
    Graph *g = create_graph(8);
    add_edge(g, 1, 1);

    MultigraphStats ms = {0};
    detect_multigraph(g, &ms);
    assert_int(ms.is_simple, 0, "case2 simple");
    assert_int(ms.loops, 1, "case2 loops");

    free_graph(g);
}

static void test_case_3_parallel(void) {
    Graph *g = create_graph(8);
    add_edge(g, 1, 2);
    add_edge(g, 1, 2);

    MultigraphStats ms = {0};
    detect_multigraph(g, &ms);
    assert_int(ms.is_simple, 0, "case3 simple");
    assert_int(ms.multiple_edges, 1, "case3 multi");

    free_graph(g);
}

static void test_case_4_two_components(void) {
    Graph *g = create_graph(8);
    add_edge(g, 1, 2);
    add_edge(g, 3, 4);

    ComponentStats cs = {0};
    count_components(g, &cs);
    assert_int(cs.num_components, 2, "case4 components");
    int ex[] = {2, 2};
    assert_sizes(cs.sizes, cs.num_components, ex, 2, "case4 sizes");
    free_component_stats(&cs);

    free_graph(g);
}

static void test_case_5_mixed(void) {
    Graph *g = create_graph(16);
    add_edge(g, 1, 2);
    add_edge(g, 2, 3);
    add_edge(g, 3, 3);
    add_edge(g, 1, 2);
    add_edge(g, 10, 11);

    MultigraphStats ms = {0};
    detect_multigraph(g, &ms);
    assert_int(ms.is_simple, 0, "case5 simple");
    assert_int(ms.loops, 1, "case5 loops");
    assert_int(ms.multiple_edges, 1, "case5 multi");

    ComponentStats cs = {0};
    count_components(g, &cs);
    assert_int(cs.num_components, 2, "case5 components");
    int ex[] = {2, 3};
    assert_sizes(cs.sizes, cs.num_components, ex, 2, "case5 sizes");
    free_component_stats(&cs);

    free_graph(g);
}

int main(void) {
    test_case_1_simple_chain();
    test_case_2_loop();
    test_case_3_parallel();
    test_case_4_two_components();
    test_case_5_mixed();

    if (failures != 0) {
        fprintf(stderr, "%d test assertion(s) failed.\n", failures);
        return 1;
    }
    puts("All tests passed.");
    return 0;
}
