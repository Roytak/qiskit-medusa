
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sylvan.h>
#include <sylvan_int.h>

#include "medusa.h"
#include "mtbdd.h"

void
medusa_init(void)
{
    init_sylvan();
    init_my_leaf(1);
}

void
medusa_destroy(void)
{
    stop_sylvan();
}

simulator_ctx_t *
medusa_simulator_ctx_init(void)
{
    simulator_ctx_t *ctx;

    ctx = calloc(1, sizeof *ctx);
    if (!ctx) {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }

    return ctx;
}

int
medusa_simulate_file(simulator_ctx_t *ctx, const char *filename)
{
    int r;
    FILE *in;

    in = fopen(filename, "r");
    if (!in) {
        fprintf(stderr, "Could not open file %s\n", filename);
        return 1;
    }

    r = sim_file(in, ctx);

    fclose(in);

    return 0 ? r : 1;
}

static size_t
medusa_get_mtbdd_leaf_count_r(MTBDD node)
{
    if (mtbdd_isleaf(node)) {
        return 1;
    } else if (node == mtbdd_true) {
        return medusa_get_mtbdd_leaf_count_r(mtbdd_gethigh(node));
    } else if (node == mtbdd_false) {
        return medusa_get_mtbdd_leaf_count_r(mtbdd_getlow(node));
    } else {
        return medusa_get_mtbdd_leaf_count_r(mtbdd_getlow(node)) +
               medusa_get_mtbdd_leaf_count_r(mtbdd_gethigh(node));
    }
}

static char *
medusa_append_char(const char *str, char c)
{
    size_t len;
    char *new_str;

    len = strlen(str);
    new_str = malloc(len + 2);
    if (!new_str) {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }
    strcpy(new_str, str);
    new_str[len] = c;
    new_str[len + 1] = '\0';
    return new_str;
}

static int
medusa_get_counts_r(MTBDD node, char **indices, double *probs, char *qubit_str, int idx)
{
    double prob;
    char *next_qubit_str1 = NULL, *next_qubit_str2 = NULL;

    if (mtbdd_isleaf(node)) {
        prob = mtbdd_getvalue(node);
        indices[idx] = strdup(qubit_str);
        if (!indices[idx]) {
            fprintf(stderr, "Memory allocation failed\n");
            return 1;
        }
        probs[idx] = prob;
    } else if (node == mtbdd_true) {
        next_qubit_str1 = medusa_append_char(qubit_str, '1');
        if (!next_qubit_str1) {
            return 1;
        }
        return medusa_get_counts_r(mtbdd_gethigh(node), indices, probs, next_qubit_str1, idx + 1);
    } else if (node == mtbdd_false) {
        next_qubit_str1 = medusa_append_char(qubit_str, '0');
        if (!next_qubit_str1) {
            return 1;
        }
        return medusa_get_counts_r(mtbdd_getlow(node), indices, probs, next_qubit_str1, idx);
    } else {
        next_qubit_str1 = medusa_append_char(qubit_str, '0');
        next_qubit_str2 = medusa_append_char(qubit_str, '1');
        if (!next_qubit_str1 || !next_qubit_str2) {
            free(next_qubit_str1);
            free(next_qubit_str2);
            return 1;
        }

        if (medusa_get_counts_r(mtbdd_getlow(node), indices, probs, next_qubit_str1, idx) ||
                medusa_get_counts_r(mtbdd_gethigh(node), indices, probs, next_qubit_str2, idx + 1)) {
            free(next_qubit_str1);
            free(next_qubit_str2);
            return 1;
        }
    }

    free(next_qubit_str1);
    free(next_qubit_str2);
    return 0;
}

int
medusa_get_counts(simulator_ctx_t *ctx, char **indices[], double **probs)
{
    size_t leaf_count;

    if (!ctx || !indices || !probs) {
        return 1;
    }

    *indices = NULL;
    *probs    = NULL;

    /* get number of leaves and allocate arrays */
    leaf_count = medusa_get_mtbdd_leaf_count_r(ctx->circuit);
    *indices = malloc(leaf_count * sizeof(**indices));
    *probs    = malloc(leaf_count * sizeof(**probs));
    if (!*indices || !*probs) {
        free(*indices);
        free(*probs);
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    /* fill arrays */
    if (medusa_get_counts_r(ctx->circuit, *indices, *probs, "", 0)) {
        free(*indices);
        free(*probs);
        *indices = NULL;
        *probs    = NULL;
        return 1;
    }

    return 0;
}

void
medusa_free_counts(char **indices, double *probs)
{
    int i;

    for (i = 0; indices[i]; i++) {
        free(indices[i]);
    }

    free(indices);
    free(probs);
}
