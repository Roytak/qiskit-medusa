
#include <math.h>
#include <string.h>

#include "assertions.h"
#include "circuit_ir.h"
#include "error.h"
#include "medusa.h"
#include "sim.h"

void
assert_equal(const char *expected_state, double prob_threshold, MTBDD *circ)
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
        error_exit("Assertion failed: expected probability of state |%s> is %f, but got %f\n",
                expected_state, prob_threshold, prob);
    }
}

void
assert_superposition(uint32_t *qubits_to_check, uint32_t nqubits_to_check, uint32_t circ_nqubits, MTBDD *circ)
{
    int r;
    char **indices = NULL;
    double *probs = NULL;

    r = medusa_get_counts(*circ, circ_nqubits, &indices, &probs);
    if (r) {
        error_exit("Error occurred while retrieving state probabilities for superposition assertion.\n");
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
            error_exit("Assertion failed: qubit %u is not in superposition (probability of being in state |1> is %f)\n",
                    qubits_to_check[i], sum);
        }
    }

    medusa_free_counts(indices, probs);
}

/**
 * Checks whether two qubits are connected through a chain of multi-qubit gate
 * interactions in the circuit (BFS on the interaction graph).
 *
 * Two qubits x and y interact if there is a multi-qubit gate acting on both,
 * or if x shares a gate with some qubit z that (transitively) interacts with y.
 */
static int
qubits_are_connected(uint32_t q1, uint32_t q2, const circuit_ir_t *ir)
{
    if (q1 == q2) return 1;

    uint32_t n = ir->n_qubits;
    uint8_t  visited[n];
    uint32_t queue[n];
    uint32_t head = 0, tail = 0;

    memset(visited, 0, n * sizeof(uint8_t));

    visited[q1] = 1;
    queue[tail++] = q1;

    while (head < tail) {
        uint32_t curr = queue[head++];

        for (size_t i = 0; i < ir->len; i++) {
            const gate_instr_t *instr = &ir->instrs[i];

            switch (instr->kind) {
            case GATE_CX:
            case GATE_CZ: {
                uint32_t qc = instr->p.controlled.qc;
                uint32_t qt = instr->p.controlled.qt;
                if (curr == qc || curr == qt) {
                    uint32_t other = (curr == qc) ? qt : qc;
                    if (other == q2) return 1;
                    if (!visited[other]) {
                        visited[other] = 1;
                        queue[tail++] = other;
                    }
                }
                break;
            }
            case GATE_CCX: {
                uint32_t qc1 = instr->p.toffoli.qc1;
                uint32_t qc2 = instr->p.toffoli.qc2;
                uint32_t qt  = instr->p.toffoli.qt;
                if (curr == qc1 || curr == qc2 || curr == qt) {
                    uint32_t others[3] = {qc1, qc2, qt};
                    for (int j = 0; j < 3; j++) {
                        if (others[j] != curr) {
                            if (others[j] == q2) return 1;
                            if (!visited[others[j]]) {
                                visited[others[j]] = 1;
                                queue[tail++] = others[j];
                            }
                        }
                    }
                }
                break;
            }
            case GATE_MCX: {
                uint32_t *qubits = instr->p.multi.qubits;
                uint32_t  nq     = instr->p.multi.n_qubits;
                int involves_curr = 0;
                for (uint32_t j = 0; j < nq; j++) {
                    if (qubits[j] == curr) { involves_curr = 1; break; }
                }
                if (involves_curr) {
                    for (uint32_t j = 0; j < nq; j++) {
                        if (qubits[j] != curr) {
                            if (qubits[j] == q2) return 1;
                            if (!visited[qubits[j]]) {
                                visited[qubits[j]] = 1;
                                queue[tail++] = qubits[j];
                            }
                        }
                    }
                }
                break;
            }
            default:
                /* Single-qubit gates, measurements, assertions, and loop
                 * control do not create qubit-qubit interactions. */
                break;
            }
        }
    }

    return 0;
}

void
assert_entanglement(uint32_t *qubits_to_check, uint32_t nqubits_to_check, const circuit_ir_t *ir, MTBDD *circ)
{
    uint8_t adjacency_matrix[nqubits_to_check][nqubits_to_check];

    if (nqubits_to_check < 2) {
        printf("Entanglement assertion requires at least 2 qubits to check, but got %u.\n", nqubits_to_check);
        return;
    }

    memset(adjacency_matrix, 0, sizeof(adjacency_matrix));

    /* build the adjacency matrix by going through pairs of qubits,
     * for each pair check if they interact with each other in the circuit (i.e. there is a gate that has them both as operands)
     * or the first one operates with another qubit that operates with the second one and so on, recursively.
     * If they interact, mark them as connected in the adjacency matrix. */
    for (uint32_t i = 0; i < nqubits_to_check; i++) {
        uint32_t q1 = qubits_to_check[i];
        for (uint32_t j = i + 1; j < nqubits_to_check; j++) {
            uint32_t q2 = qubits_to_check[j];

            if (qubits_are_connected(q1, q2, ir)) {
                adjacency_matrix[i][j] = 1;
                adjacency_matrix[j][i] = 1;
            }
        }
    }

    for (uint32_t i = 0; i < nqubits_to_check; i++) {
        for (uint32_t j = i + 1; j < nqubits_to_check; j++) {
            if (!adjacency_matrix[i][j]) {
                error_exit("Assertion failed: qubits %u and %u are not entangled\n", qubits_to_check[i], qubits_to_check[j]);
            }
        }
    }
}
