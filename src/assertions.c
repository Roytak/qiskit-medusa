
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
        error_set(ir, -1, "Invalid qubit index in entanglement assertion: %u or %u is out of bounds (circuit has %u qubits)\n",
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
assert_entanglement(circuit_ir_t *ir, uint32_t *qubits_to_check, uint32_t nqubits_to_check, MTBDD *circ)
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
                error_set(ir, -1, "Assertion failed: qubits %u and %u are not entangled\n", q1, q2);
                return 1;
            }
        }
    }

    return 0;
}
