
#ifndef ASSERTIONS_H
#define ASSERTIONS_H

#include <stdint.h>

#include "mtbdd.h"

/**
 * @brief Asserts that the probability of the given state in the circuit is equal to the given threshold up to a small epsilon.
 *
 * @param[in] expected_state The expected state string (e.g. "010" for a 3-qubit state).
 * @param[in] prob_threshold The expected probability of the state.
 * @param[in] circ MTBDD representing the circuit state.
 */
void assert_equal(const char *expected_state, double prob_threshold, MTBDD *circ);

/**
 * @brief Asserts that the specified qubits are in a superposition state.
 *
 * @param[in] qubits_to_check Array of qubit indices to check for superposition.
 * @param[in] nqubits_to_check Number of qubits to check.
 * @param[in] circ_nqubits Total number of qubits in the circuit.
 * @param[in] circ MTBDD representing the circuit state.
 */
void assert_superposition(uint32_t *qubits_to_check, uint32_t nqubits_to_check, uint32_t circ_nqubits, MTBDD *circ);

#endif /* ASSERTIONS_H */
