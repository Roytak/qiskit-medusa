
#ifndef ASSERTIONS_H
#define ASSERTIONS_H

#include <stdint.h>

#include "circuit_ir.h"
#include "mtbdd.h"

/**
 * @brief Asserts that the probability of the given state in the circuit is equal to the given threshold up to a small epsilon.
 *
 * @param[in] ir Circuit intermediate representation (used for error reporting).
 * @param[in] expected_state The expected state string (e.g. "010" for a 3-qubit state).
 * @param[in] prob_threshold The expected probability of the state.
 * @param[in] circ MTBDD representing the circuit state.
 * @return 0 if the assertion passes, non-zero if it fails.
 */
int assert_equal(circuit_ir_t *ir, const char *expected_state, double prob_threshold, MTBDD *circ);

/**
 * @brief Asserts that the specified qubits are in a superposition state.
 *
 * @param[in] ir Circuit intermediate representation (used for error reporting).
 * @param[in] qubits_to_check Array of qubit indices to check for superposition.
 * @param[in] nqubits_to_check Number of qubits to check.
 * @param[in] circ_nqubits Total number of qubits in the circuit.
 * @param[in] circ MTBDD representing the circuit state.
 * @return 0 if the assertion passes, non-zero if it fails.
 */
int assert_superposition(circuit_ir_t *ir, uint32_t *qubits_to_check, uint32_t nqubits_to_check, uint32_t circ_nqubits, MTBDD *circ);

/**
 * @brief Asserts that the specified qubits have interacted in the circuit.
 *
 * A pair of qubits A and B are considered to have interacted, if either:
 * - there is a gate in the circuit that has both A and B as operands, or
 * - there is a chain of gates connecting A to B through other qubits, recursively.
 *
 * @param[in] ir Circuit intermediate representation (used for error reporting).
 * @param[in] qubits_to_check Array of qubit indices to check for interaction connectivity.
 * @param[in] nqubits_to_check Number of qubits to check.
 * @param[in] circ MTBDD representing the circuit state (unused by this check).
 * @return 0 if the assertion passes, non-zero if it fails.
 */
int assert_interact(circuit_ir_t *ir, uint32_t *qubits_to_check, uint32_t nqubits_to_check, MTBDD *circ);

/**
 * @brief Asserts that two qubits are entangled using a PPT-based check.
 *
 * This assertion currently supports exactly two qubits.
 *
 * @param[in] ir Circuit intermediate representation (used for error reporting).
 * @param[in] qubits_to_check Array with the two qubit indices to check.
 * @param[in] nqubits_to_check Number of qubits in @p qubits_to_check.
 * @param[in] circ MTBDD representing the circuit state.
 * @return 0 if the assertion passes, non-zero if it fails.
 */
int assert_entanglement(circuit_ir_t *ir, uint32_t *qubits_to_check, uint32_t nqubits_to_check, MTBDD *circ);

#endif /* ASSERTIONS_H */
