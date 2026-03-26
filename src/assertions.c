
#include <complex.h>
#include <math.h>
#include <string.h>

#include "assertions.h"
#include "circuit_ir.h"
#include "error.h"
#include "medusa.h"
#include "sim.h"

int
assert_equal(circuit_ir_t *ir, const char *expected_state, double prob_threshold, MTBDD *circ)
{
    cnum *value;
    double prob;
    MTBDD node = *circ;

    for (int i = 0; i < strlen(expected_state); i++) {
        if (expected_state[i] == '0') {
            /* go low */
            node = mtbdd_getlow(node);
        } else {
            /* go high */
            node = mtbdd_gethigh(node);
        }
    }

    value = (cnum *)mtbdd_getvalue(node);
    prob = calculate_prob(value);

    if (fabs(prob - prob_threshold) > EPSILON) {
        error_set(ir, -1, "Assertion failed: expected probability of state |%s> is %f, but got %f\n",
                expected_state, prob_threshold, prob);
        return 1;
    }

    return 0;
}

int
assert_superposition(circuit_ir_t *ir, uint32_t *qubits_to_check, uint32_t nqubits_to_check, uint32_t circ_nqubits, MTBDD *circ)
{
    int r;
    char **indices = NULL;
    double *probs = NULL;

    r = medusa_get_counts(*circ, circ_nqubits, &indices, &probs);
    if (r) {
        error_set(ir, -1, "Error occurred while retrieving state probabilities for superposition assertion.\n");
        return 1;
    }

    /* a qubit is in superposition if the probability of it being in state |1> is between 0 and 1 (exclusive),
     * so go through all the states and sum the probabilities of states where the i-th qubit is in state |1> */
    for (uint32_t i = 0; i < nqubits_to_check; i++) {
        double sum = 0.0;
        for (uint32_t j = 0; indices[j]; j++) {
            if (indices[j][qubits_to_check[i]] == '1') {
                sum += probs[j];
            }
        }

        if ((sum == 0) || (sum == 1)) {
            error_set(ir, -1, "Assertion failed: qubit %u is not in superposition (probability of being in state |1> is %f)\n",
                    qubits_to_check[i], sum);
            medusa_free_counts(indices, probs);
            return 1;
        }
    }

    medusa_free_counts(indices, probs);
    return 0;
}

/**
 * Check if qubits q1 and q2 are connected through interactions in the circuit IR
 * (i.e. there is a gate that has them both as operands).
 */
static int
qubits_are_connected(uint32_t q1, uint32_t q2, circuit_ir_t *ir)
{
    if (q1 >= ir->n_qubits || q2 >= ir->n_qubits) {
        error_set(ir, -1, "Invalid qubit index in interaction assertion: %u or %u is out of bounds (circuit has %u qubits)\n",
                q1, q2, ir->n_qubits);
        return -1;  /* error */
    }

    if (!ir->qubit_interactions) {
        return 0;  /* no interactions at all */
    }

    if (q1 == q2) {
        /* same qubit, trivially connected */
        return 1;
    }

    if (ir->qubit_interactions[q1][q2]) {
        /* directly interact */
        return 1;
    }

    /* Check if they interact through a chain of interactions with other qubits in qubits_to_check. */
    uint8_t visited[ir->n_qubits];
    uint32_t queue[ir->n_qubits];
    uint32_t head = 0, tail = 0;

    memset(visited, 0, ir->n_qubits * sizeof(uint8_t));
    visited[q1] = 1;
    queue[tail++] = q1;
    while (head < tail) {
        uint32_t curr = queue[head++];

        for (uint32_t other = 0; other < ir->n_qubits; other++) {
            if (ir->qubit_interactions[curr][other] && !visited[other]) {
                if (other == q2) {
                    return 1;
                }
                visited[other] = 1;
                queue[tail++] = other;
            }
        }
    }

    return 0;
}

int
assert_interact(circuit_ir_t *ir, uint32_t *qubits_to_check, uint32_t nqubits_to_check, MTBDD *circ)
{
    /* Check that every pair of qubits in qubits_to_check interacts with each other */
    for (uint32_t i = 0; i < nqubits_to_check; i++) {
        uint32_t q1 = qubits_to_check[i];
        for (uint32_t j = i + 1; j < nqubits_to_check; j++) {
            uint32_t q2 = qubits_to_check[j];

            int connected = qubits_are_connected(q1, q2, ir);
            if (connected < 0) {
                /* error already set by qubits_are_connected */
                return 1;
            }
            if (!connected) {
                error_set(ir, -1, "Assertion failed: qubits %u and %u are not connected by interactions\n", q1, q2);
                return 1;
            }
        }
    }

    return 0;
}

static double complex
algebraic2stdcomplex(cnum* algebraic)
{
    if (!algebraic) return 0.0 + 0.0 * I;

    mpf_t a, b, c, d;
    double c_a, c_b, c_c, c_d;
    double real_part, imag_part;

    mpf_inits(a, b, c, d, NULL);
    mpf_set_z(a, algebraic->a);
    mpf_set_z(b, algebraic->b);
    mpf_set_z(c, algebraic->c);
    mpf_set_z(d, algebraic->d);

    mp_bitcnt_t shift_cnt = mpz_get_ui(c_k) >> 1;

    if (mpz_even_p(c_k) != 0) {
        mpf_div_2exp(a, a, shift_cnt);
        mpf_div_2exp(b, b, shift_cnt);
        mpf_div_2exp(c, c, shift_cnt);
        mpf_div_2exp(d, d, shift_cnt);

        c_a = mpf_get_d(a); c_b = mpf_get_d(b);
        c_c = mpf_get_d(c); c_d = mpf_get_d(d);

        // Calculate amplitudes WITHOUT squaring them
        real_part = c_a + c_b * M_SQRT1_2 - c_d * M_SQRT1_2;
        imag_part = c_c + c_b * M_SQRT1_2 + c_d * M_SQRT1_2;
    }
    else {
        mpf_div_2exp(a, a, shift_cnt);
        mpf_div_2exp(b, b, shift_cnt + 1);
        mpf_div_2exp(c, c, shift_cnt);
        mpf_div_2exp(d, d, shift_cnt + 1);

        c_a = mpf_get_d(a); c_b = mpf_get_d(b);
        c_c = mpf_get_d(c); c_d = mpf_get_d(d);

        // Calculate amplitudes WITHOUT squaring them
        real_part = c_a * M_SQRT1_2 + c_b - c_d;
        imag_part = c_c * M_SQRT1_2 + c_b + c_d;
    }

    mpf_clears(a, b, c, d, NULL);

    // Return standard C complex number
    return real_part + imag_part * I;
}

static int
medusa_get_amplitudes_r(const MTBDD node, char **indices, double complex *amps,
        char *path_buffer, int depth, size_t *current_idx)
{
    cnum *value;

    /* Base case: we hit a leaf node */
    if (mtbdd_isleaf(node)) {
        /* terminate the string built up in the path buffer */
        path_buffer[depth] = '\0';

        /* store the probability */
        value = (cnum *)mtbdd_getvalue(node);
        amps[*current_idx] = algebraic2stdcomplex(value);

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
    if (medusa_get_amplitudes_r(mtbdd_getlow(node), indices, amps,
                path_buffer, depth + 1, current_idx)) {
        return 1;
    }

    /* traverse the '1' branch */
    path_buffer[depth] = '1';
    if (medusa_get_amplitudes_r(mtbdd_gethigh(node), indices, amps,
                path_buffer, depth + 1, current_idx)) {
        return 1;
    }

    return 0;
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

int
medusa_get_amplitudes(MTBDD mtbdd, int num_qubits, char **indices[], double complex **amps)
{
    size_t leaf_count;
    char *path_buffer;
    size_t current_idx = 0;
    int max_depth;

    if ((num_qubits <= 0) || !indices || !amps) {
        return 1;
    }

    *indices = NULL;
    *amps    = NULL;

    max_depth = num_qubits;

    /* get number of leaves and allocate arrays */
    leaf_count = medusa_get_mtbdd_leaf_count_r(mtbdd);
    if (leaf_count == 0) {
        /* empty tree */
        return 0;
    }

    /* allocate result arrays */
    *indices = malloc((leaf_count + 1) * sizeof(**indices));
    *amps = malloc((leaf_count + 1) * sizeof(**amps));

    /* allocate buffer for path string */
    path_buffer = malloc((max_depth + 1) * sizeof(*path_buffer));

    if (!*indices || !*amps || !path_buffer) {
        free(*indices);
        free(*amps);
        free(path_buffer);
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    /* get counts recursively */
    if (medusa_get_amplitudes_r(mtbdd, *indices, *amps, path_buffer, 0, &current_idx)) {
        free(*indices);
        free(*amps);
        free(path_buffer);
        *indices = NULL;
        *amps = NULL;
        return 1;
    }

    /* NULL-terminate the arrays */
    (*indices)[current_idx] = NULL;
    (*amps)[current_idx] = 0.0;

    free(path_buffer);
    return 0;
}

/**
 * @brief Extracts the 2 bits for qA and qB from the state string and turns them into an index 0-3.
 *
 * 00 -> 0, 01 -> 1, 10 -> 2, 11 -> 3
 *
 * @param[in] state_str the string representing the state (e.g. "0101")
 * @param[in] qA index of the first qubit to check
 * @param[in] qB index of the second qubit to check
 * @return index corresponding to the state of qA and qB
 */
static int
get_matrix_index(const char *state_str, int qA, int qB) {
    int bitA = state_str[qA] - '0';
    int bitB = state_str[qB] - '0';
    return (bitA << 1) | bitB;
}

/**
 * @brief Checks if two state strings match everywhere EXCEPT at qA and qB.
 *
 * @param[in] str1 first state string (e.g. "0101")
 * @param[in] str2 second state string (e.g. "0001")
 * @param[in] qA index of the first qubit to ignore
 * @param[in] qB index of the second qubit to ignore
 * @param[in] num_qubits total number of qubits (length of the state strings)
 * @return true if the strings match at all positions except qA and qB, false otherwise
 */
bool
match_other_qubits(const char *str1, const char *str2, int qA, int qB, int num_qubits) {
    for (int i = 0; i < num_qubits; i++) {
        if (i == qA || i == qB) {
            /* ignore the qubits we are keeping */
            continue;
        }

        if (str1[i] != str2[i]) {
            /* mismatch */
            return false;
        }
    }

    return true;
}

/**
 * @brief Constructs the 4x4 density matrix for qubits qA and qB from the state paths and amplitudes.
 *
 * @param[in] indices array of state strings (e.g. "0101") for each active path in the MTBDD
 * @param[in] amps array of complex amplitudes corresponding to each state string
 * @param[in] leaf_count number of active paths (length of indices and amps arrays)
 * @param[in] num_qubits total number of qubits (length of each state string
 * @param[in] qA index of the first qubit to check
 * @param[in] qB index of the second qubit to check
 * @param[out] rho the resulting 4x4 density matrix for qubits qA and qB
 */
static void
density_matrix_from_paths(char **indices, double complex *amps, int leaf_count, int num_qubits,
    int qA, int qB, double complex rho[4][4])
{
    int i, j;

    // initialize matrix to 0
    for(i=0; i<4; i++) {
        for(j=0; j<4; j++) {
            rho[i][j] = 0.0 + 0.0 * I;
        }
    }

    // loop over the active state paths
    for (i = 0; i < leaf_count; i++) {

        // find which row (0-3) this path belongs to based on qA and qB
        int row = get_matrix_index(indices[i], qA, qB);

        for (j = 0; j < leaf_count; j++) {

            // To do the partial trace, the "other" qubits must be identical.
            // If they are identical, we multiply amp[i] by the complex conjugate of amp[j].
            if (match_other_qubits(indices[i], indices[j], qA, qB, num_qubits)) {

                int col = get_matrix_index(indices[j], qA, qB);

                // Add to the matrix: Amplitude * Complex_Conjugate(Amplitude)
                rho[row][col] += amps[i] * conj(amps[j]);
            }
        }
    }
}

/**
 * @brief Computes the partial transpose of a 2-qubit density matrix.
 *
 * @param[in] rho the original 4x4 density matrix
 * @param[out] rho_ppt the resulting 4x4 matrix after partial transpose
 */
void
partial_transpose(double complex rho[4][4], double complex rho_ppt[4][4]) {
    // Copy the original matrix
    for(int i=0; i<4; i++) {
        for(int j=0; j<4; j++) {
            rho_ppt[i][j] = rho[i][j];
        }
    }

    // Swap the inner elements to transpose Qubit B
    // Swap (0,1) with (1,0)
    rho_ppt[0][1] = rho[1][0];
    rho_ppt[1][0] = rho[0][1];

    // Swap (2,3) with (3,2)
    rho_ppt[2][3] = rho[3][2];
    rho_ppt[3][2] = rho[2][3];

    // Swap (0,3) with (1,2)
    rho_ppt[0][3] = rho[1][2];
    rho_ppt[1][2] = rho[0][3];

    // Swap (2,1) with (3,0)
    rho_ppt[2][1] = rho[3][0];
    rho_ppt[3][0] = rho[2][1];
}

/* Helper: 2x2 Principal Minor Determinant */
static double real_det2x2(double complex m[4][4], int i, int j) {
    double complex det = m[i][i] * m[j][j] - m[i][j] * m[j][i];
    return creal(det); // Determinants of Hermitian principal minors are strictly real
}

/* Helper: 3x3 Principal Minor Determinant */
static double real_det3x3(double complex m[4][4], int i, int j, int k) {
    double complex det =
          m[i][i] * (m[j][j] * m[k][k] - m[j][k] * m[k][j])
        - m[i][j] * (m[j][i] * m[k][k] - m[j][k] * m[k][i])
        + m[i][k] * (m[j][i] * m[k][j] - m[j][j] * m[k][i]);
    return creal(det);
}

/* Helper: Full 4x4 Determinant (Laplace expansion along row 0) */
static double real_det4x4(double complex m[4][4]) {
    double complex det = 0;
    det += m[0][0] * (m[1][1]*(m[2][2]*m[3][3] - m[2][3]*m[3][2]) - m[1][2]*(m[2][1]*m[3][3] - m[2][3]*m[3][1]) + m[1][3]*(m[2][1]*m[3][2] - m[2][2]*m[3][1]));
    det -= m[0][1] * (m[1][0]*(m[2][2]*m[3][3] - m[2][3]*m[3][2]) - m[1][2]*(m[2][0]*m[3][3] - m[2][3]*m[3][0]) + m[1][3]*(m[2][0]*m[3][2] - m[2][2]*m[3][0]));
    det += m[0][2] * (m[1][0]*(m[2][1]*m[3][3] - m[2][3]*m[3][1]) - m[1][1]*(m[2][0]*m[3][3] - m[2][3]*m[3][0]) + m[1][3]*(m[2][0]*m[3][1] - m[2][1]*m[3][0]));
    det -= m[0][3] * (m[1][0]*(m[2][1]*m[3][2] - m[2][2]*m[3][1]) - m[1][1]*(m[2][0]*m[3][2] - m[2][2]*m[3][0]) + m[1][2]*(m[2][0]*m[3][1] - m[2][1]*m[3][0]));
    return creal(det);
}

/* The Core Check: Returns 1 if entangled, 0 if separable */
static int is_ppt_entangled(double complex rho_ppt[4][4]) {
    double tol = -1e-10; // Tolerance for floating point exactness

    // 1. Check 1x1 minors (the diagonal)
    for (int i = 0; i < 4; i++) {
        if (creal(rho_ppt[i][i]) < tol) return 1;
    }

    // 2. Check 2x2 principal minors
    if (real_det2x2(rho_ppt, 0, 1) < tol) return 1;
    if (real_det2x2(rho_ppt, 0, 2) < tol) return 1;
    if (real_det2x2(rho_ppt, 0, 3) < tol) return 1;
    if (real_det2x2(rho_ppt, 1, 2) < tol) return 1;
    if (real_det2x2(rho_ppt, 1, 3) < tol) return 1;
    if (real_det2x2(rho_ppt, 2, 3) < tol) return 1;

    // 3. Check 3x3 principal minors
    if (real_det3x3(rho_ppt, 0, 1, 2) < tol) return 1;
    if (real_det3x3(rho_ppt, 0, 1, 3) < tol) return 1;
    if (real_det3x3(rho_ppt, 0, 2, 3) < tol) return 1;
    if (real_det3x3(rho_ppt, 1, 2, 3) < tol) return 1;

    // 4. Check full 4x4 determinant
    if (real_det4x4(rho_ppt) < tol) return 1;

    // If we made it here, no negative eigenvalues exist.
    return 0;
}

int
assert_entanglement(circuit_ir_t *ir, uint32_t *qubits_to_check, uint32_t nqubits_to_check, MTBDD *circ)
{
    char **indices = NULL;
    double complex *amps = NULL;
    double complex rho[4][4];
    double complex rho_ppt[4][4];
    int is_entangled, ret = 0;
    size_t actual_leaf_count = 0; // Added variable to hold the true path count

    if (!qubits_to_check) {
        error_set(ir, -1, "Invalid input: qubits_to_check is NULL\n");
        return 1;
    }
    if (nqubits_to_check != 2) {
        error_set(ir, -1, "Invalid input: nqubits_to_check must be exactly 2 for entanglement assertion\n");
        return 1;
    }

    /* get the amplitudes of all states in the MTBDD */
    if (medusa_get_amplitudes(*circ, ir->n_qubits, &indices, &amps)) {
        error_set(ir, -1, "Error occurred while retrieving state amplitudes for entanglement assertion.\n");
        return 1;
    }

    while (indices[actual_leaf_count] != NULL) {
        actual_leaf_count++;
    }

    /* construct the 4x4 density matrix for the 2 qubits we are checking */
    density_matrix_from_paths(indices, amps, actual_leaf_count, ir->n_qubits, qubits_to_check[0], qubits_to_check[1], rho);

    /* compute the partial transpose of the density matrix */
    partial_transpose(rho, rho_ppt);

    /* execute the Sylvester's Criterion check */
    is_entangled = is_ppt_entangled(rho_ppt);

    if (!is_entangled) {
        error_set(ir, -1, "Assertion failed: qubits %u and %u are not entangled\n",
                qubits_to_check[0], qubits_to_check[1]);
        ret = 1;
    }

    for (size_t i = 0; indices[i]; i++) {
        free(indices[i]);
    }
    free(indices);
    free(amps);

    return ret;
}
