
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sylvan.h>
#include <sylvan_int.h>

#include "circuit_ir.h"
#include "error.h"
#include "medusa.h"
#include "mtbdd.h"
#include "sim.h"
#include "symb_utils.h"

#define CHECK_IR_HANDLE_RET(ir) \
    if (!(ir)) { \
        fprintf(stderr, "Invalid circuit IR handle\n"); \
        return 1; \
    }

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

const char *
medusa_get_last_error(circuit_ir_t *ir, int *line)
{
    const char *msg;
    int l;

    if (!ir) {
        fprintf(stderr, "Invalid circuit IR handle\n");
        if (line) {
            *line = -1;
        }
        return "Invalid circuit IR handle";
    }

    error_get_last(ir, &msg, &l);
    if (line) {
        *line = l;
    }
    return msg;
}

void
medusa_clear_error(circuit_ir_t *ir)
{
    if (!ir) {
        fprintf(stderr, "Invalid circuit IR handle\n");
        return;
    }

    error_clear(ir);
}

circuit_ir_t *
medusa_circuit_create(void)
{
    return circuit_ir_create();
}

void
medusa_circuit_destroy(circuit_ir_t *ir)
{
    circuit_ir_destroy(ir);
}

/* ------------------------------------------------------------------ */
/*  Qubit / bit register setup                                         */
/* ------------------------------------------------------------------ */

int
medusa_set_qubits(circuit_ir_t *ir, uint32_t n)
{
    CHECK_IR_HANDLE_RET(ir);

    circuit_ir_set_qubits(ir, n);
    return 0;
}

int
medusa_set_bits(circuit_ir_t *ir, uint32_t n)
{
    CHECK_IR_HANDLE_RET(ir);

    circuit_ir_set_bits(ir, n);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Single-qubit gates                                                 */
/* ------------------------------------------------------------------ */

int
medusa_add_x(circuit_ir_t *ir, uint32_t qt)
{
    CHECK_IR_HANDLE_RET(ir);

    circuit_ir_add_x(ir, qt);
    return 0;
}

int
medusa_add_y(circuit_ir_t *ir, uint32_t qt)
{
    CHECK_IR_HANDLE_RET(ir);

    circuit_ir_add_y(ir, qt);
    return 0;
}

int
medusa_add_z(circuit_ir_t *ir, uint32_t qt)
{
    CHECK_IR_HANDLE_RET(ir);

    circuit_ir_add_z(ir, qt);
    return 0;
}

int
medusa_add_h(circuit_ir_t *ir, uint32_t qt)
{
    CHECK_IR_HANDLE_RET(ir);

    circuit_ir_add_h(ir, qt);
    return 0;
}

int
medusa_add_s(circuit_ir_t *ir, uint32_t qt)
{
    CHECK_IR_HANDLE_RET(ir);

    circuit_ir_add_s(ir, qt);
    return 0;
}

int
medusa_add_t(circuit_ir_t *ir, uint32_t qt)
{
    CHECK_IR_HANDLE_RET(ir);

    circuit_ir_add_t(ir, qt);
    return 0;
}

int
medusa_add_rx_pihalf(circuit_ir_t *ir, uint32_t qt)
{
    CHECK_IR_HANDLE_RET(ir);

    circuit_ir_add_rx_pihalf(ir, qt);
    return 0;
}

int
medusa_add_ry_pihalf(circuit_ir_t *ir, uint32_t qt)
{
    CHECK_IR_HANDLE_RET(ir);

    circuit_ir_add_ry_pihalf(ir, qt);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Two-qubit gates                                                    */
/* ------------------------------------------------------------------ */

int
medusa_add_cx(circuit_ir_t *ir, uint32_t qc, uint32_t qt)
{
    CHECK_IR_HANDLE_RET(ir);

    circuit_ir_add_cx(ir, qc, qt);
    return 0;
}

int
medusa_add_cz(circuit_ir_t *ir, uint32_t qc, uint32_t qt)
{
    CHECK_IR_HANDLE_RET(ir);

    circuit_ir_add_cz(ir, qc, qt);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Three-qubit gates                                                  */
/* ------------------------------------------------------------------ */

int
medusa_add_ccx(circuit_ir_t *ir, uint32_t qc1, uint32_t qc2, uint32_t qt)
{
    CHECK_IR_HANDLE_RET(ir);

    circuit_ir_add_ccx(ir, qc1, qc2, qt);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Multi-qubit gates                                                  */
/* ------------------------------------------------------------------ */

int
medusa_add_mcx(circuit_ir_t *ir, uint32_t *qubits, uint32_t n_qubits)
{
    if (!ir || !qubits || n_qubits == 0) {
        fprintf(stderr, "Invalid arguments to medusa_add_mcx\n");
        return 1;
    }

    /* circuit_ir_add_mcx takes ownership, so we must duplicate */
    uint32_t *dup = malloc(n_qubits * sizeof(uint32_t));
    if (!dup) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    memcpy(dup, qubits, n_qubits * sizeof(uint32_t));

    circuit_ir_add_mcx(ir, dup, n_qubits);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Measurement                                                        */
/* ------------------------------------------------------------------ */

int
medusa_add_measure(circuit_ir_t *ir, uint32_t qt, uint32_t ct)
{
    CHECK_IR_HANDLE_RET(ir);

    circuit_ir_add_measure(ir, qt, ct);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Assertions                                                        */
/* ------------------------------------------------------------------ */

int
medusa_add_assert_eq(circuit_ir_t *ir, const char *state_str, double prob_threshold)
{
    if (!ir || !state_str) {
        fprintf(stderr, "Invalid arguments to medusa_add_assert_eq\n");
        return 1;
    }

    char *dup = strdup(state_str);
    if (!dup) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    circuit_ir_add_assert_eq(ir, dup, prob_threshold);
    return 0;
}

int
medusa_add_assert_sup(circuit_ir_t *ir, const uint32_t *qubits, uint32_t n_qubits)
{
    if (!ir || !qubits || n_qubits == 0) {
        fprintf(stderr, "Invalid arguments to medusa_add_assert_sup\n");
        return 1;
    }

    uint32_t *dup = malloc(n_qubits * sizeof(uint32_t));
    if (!dup) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    memcpy(dup, qubits, n_qubits * sizeof(uint32_t));

    circuit_ir_add_assert_sup(ir, dup, n_qubits);
    return 0;
}

int
medusa_add_assert_interact(circuit_ir_t *ir, const uint32_t *qubits, uint32_t n_qubits)
{
    if (!ir || !qubits || n_qubits == 0) {
        fprintf(stderr, "Invalid arguments to medusa_add_assert_interact\n");
        return 1;
    }

    uint32_t *dup = malloc(n_qubits * sizeof(uint32_t));
    if (!dup) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    memcpy(dup, qubits, n_qubits * sizeof(uint32_t));

    circuit_ir_add_assert_interact(ir, dup, n_qubits);
    return 0;
}

int
medusa_add_assert_ent(circuit_ir_t *ir, const uint32_t *qubits, uint32_t n_qubits)
{
    if (!ir || !qubits || n_qubits == 0) {
        fprintf(stderr, "Invalid arguments to medusa_add_assert_ent\n");
        return 1;
    }

    uint32_t *dup = malloc(n_qubits * sizeof(uint32_t));
    if (!dup) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    memcpy(dup, qubits, n_qubits * sizeof(uint32_t));

    circuit_ir_add_assert_ent(ir, dup, n_qubits);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Circuit simulation from IR handle                                  */
/* ------------------------------------------------------------------ */

int
medusa_simulate_circuit(circuit_ir_t *ir, int symbolic, MTBDD *mtbdd)
{
    if (!ir || !mtbdd) {
        fprintf(stderr, "Invalid arguments to medusa_simulate_circuit\n");
        return 1;
    }

    return simulate_ir(ir, symbolic, mtbdd) ? 0 : 1;
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
