
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sylvan.h>
#include <sylvan_int.h>

#include "medusa.h"
#include "mtbdd.h"
#include "sim.h"
#include "symb_utils.h"

void
medusa_init(void)
{
    init_sylvan();
    init_my_leaf(1);
    init_sylvan_symb();
}

void
medusa_destroy(void)
{
    stop_sylvan();
}

int
medusa_simulate_file(const char *filename, int symbolic, MTBDD *mtbdd)
{
    int r;
    FILE *in;

    if (!filename || !mtbdd) {
        fprintf(stderr, "Invalid arguments to medusa_simulate_file\n");
        return 1;
    }

    in = fopen(filename, "r");
    if (!in) {
        fprintf(stderr, "Could not open file %s\n", filename);
        return 1;
    }

    r = sim_file(in, symbolic, mtbdd);

    fclose(in);

    return r ? 0 : 1;
}

/**
 * @brief Recursively counts the number of leaves in an MTBDD.
 *
 * @param[in] node The MTBDD node to start counting from.
 * @return The number of leaves in the MTBDD.
 */
static size_t
medusa_get_mtbdd_leaf_count_r(const MTBDD node)
{
    if (mtbdd_isleaf(node)) {
        return 1;
    }

    return medusa_get_mtbdd_leaf_count_r(mtbdd_getlow(node)) +
           medusa_get_mtbdd_leaf_count_r(mtbdd_gethigh(node));
}

static int
medusa_get_counts_r(const MTBDD node, char **indices, double *probs,
        char *path_buffer, int depth, size_t *current_idx)
{
    cnum *value;

    /* Base case: we hit a leaf node */
    if (mtbdd_isleaf(node)) {
        /* terminate the string built up in the path buffer */
        path_buffer[depth] = '\0';

        /* store the probability */
        value = (cnum *)mtbdd_getvalue(node);
        probs[*current_idx] = calculate_prob(value);

        /* store the qubit string */
        indices[*current_idx] = strdup(path_buffer);
        if (!indices[*current_idx]) {
            fprintf(stderr, "Memory allocation failed\n");
            return 1;
        }

        (*current_idx)++;
        return 0;
    }

    /* Recursion */

    /* traverse the '0' branch */
    path_buffer[depth] = '0';
    if (medusa_get_counts_r(mtbdd_getlow(node), indices, probs,
                path_buffer, depth + 1, current_idx)) {
        return 1;
    }

    /* traverse the '1' branch */
    path_buffer[depth] = '1';
    if (medusa_get_counts_r(mtbdd_gethigh(node), indices, probs,
                path_buffer, depth + 1, current_idx)) {
        return 1;
    }

    return 0;
}

int
medusa_get_counts(MTBDD mtbdd, int num_qubits, char **indices[], double **probs)
{
    size_t leaf_count;
    char *path_buffer;
    size_t current_idx = 0;
    int max_depth;

    if ((num_qubits <= 0) || !indices || !probs) {
        return 1;
    }

    *indices = NULL;
    *probs    = NULL;

    max_depth = num_qubits;

    /* get number of leaves and allocate arrays */
    leaf_count = medusa_get_mtbdd_leaf_count_r(mtbdd);
    if (leaf_count == 0) {
        /* empty tree */
        return 0;
    }

    /* allocate result arrays */
    *indices = malloc((leaf_count + 1) * sizeof(**indices));
    *probs = malloc((leaf_count + 1) * sizeof(**probs));

    /* allocate buffer for path string */
    path_buffer = malloc((max_depth + 1) * sizeof(*path_buffer));

    if (!*indices || !*probs || !path_buffer) {
        free(*indices);
        free(*probs);
        free(path_buffer);
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    /* get counts recursively */
    if (medusa_get_counts_r(mtbdd, *indices, *probs, path_buffer, 0, &current_idx)) {
        free(*indices);
        free(*probs);
        free(path_buffer);
        *indices = NULL;
        *probs = NULL;
        return 1;
    }

    /* NULL-terminate the arrays */
    (*indices)[current_idx] = NULL;
    (*probs)[current_idx] = 0.0;

    free(path_buffer);
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
