
#include <math.h>
#include <string.h>

#include "assertions.h"
#include "sim.h"
#include "medusa.h"
#include "error.h"

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
