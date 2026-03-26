
#ifndef _MEDUSA_H_
#define _MEDUSA_H_

#include <stdint.h>

#include "circuit_ir.h"

/* ------------------------------------------------------------------ */
/*  Error reporting                                                    */
/* ------------------------------------------------------------------ */

/**
 * @brief Returns the last error message set during simulation.
 *
 * @param[in] ir The circuit IR handle to query for the last error.
 * @param[out] line The line number of the error, or -1 if not available.
 * @return The last error message, or an empty string if no error.
 */
const char *medusa_get_last_error(circuit_ir_t *ir, int *line);

/**
 * @brief Clears the last error message.
 *
 * @param[in] ir The circuit IR handle to clear the error for.
 */
void medusa_clear_error(circuit_ir_t *ir);

/* ------------------------------------------------------------------ */
/*  Library lifecycle                                                   */
/* ------------------------------------------------------------------ */

/**
 * @brief Initializes the Medusa simulator.
 */
void medusa_init(void);

/**
 * @brief Destroys the Medusa simulator and frees associated resources.
 */
void medusa_destroy(void);

/* ------------------------------------------------------------------ */
/*  Circuit handle lifecycle                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief Creates a new, empty circuit IR handle.
 *
 * @return Newly created circuit IR, or NULL on failure.
 *         The caller must free it with ::medusa_circuit_destroy().
 */
circuit_ir_t *medusa_circuit_create(void);

/**
 * @brief Destroys a circuit IR handle and frees all associated memory.
 *
 * @param[in] ir The circuit IR handle to destroy.
 */
void medusa_circuit_destroy(circuit_ir_t *ir);

/* ------------------------------------------------------------------ */
/*  Qubit / bit register setup                                         */
/* ------------------------------------------------------------------ */

/**
 * @brief Sets the number of qubits in the circuit.
 *
 * @param[in] ir  The circuit IR handle.
 * @param[in] n   Number of qubits.
 * @return 0 on success, non-zero on failure.
 */
int medusa_set_qubits(circuit_ir_t *ir, uint32_t n);

/**
 * @brief Sets the number of classical bits in the circuit.
 *
 * @param[in] ir  The circuit IR handle.
 * @param[in] n   Number of classical bits.
 * @return 0 on success, non-zero on failure.
 */
int medusa_set_bits(circuit_ir_t *ir, uint32_t n);

/* ------------------------------------------------------------------ */
/*  Single-qubit gates                                                 */
/* ------------------------------------------------------------------ */

/** @name Single-qubit gates
 *  Each adds the corresponding gate to the circuit.
 *  @param[in] ir  The circuit IR handle.
 *  @param[in] qt  Target qubit index.
 *  @return 0 on success, non-zero on failure.
 */
///@{
int medusa_add_x        (circuit_ir_t *ir, uint32_t qt);
int medusa_add_y        (circuit_ir_t *ir, uint32_t qt);
int medusa_add_z        (circuit_ir_t *ir, uint32_t qt);
int medusa_add_h        (circuit_ir_t *ir, uint32_t qt);
int medusa_add_s        (circuit_ir_t *ir, uint32_t qt);
int medusa_add_t        (circuit_ir_t *ir, uint32_t qt);
int medusa_add_rx_pihalf(circuit_ir_t *ir, uint32_t qt);
int medusa_add_ry_pihalf(circuit_ir_t *ir, uint32_t qt);
///@}

/* ------------------------------------------------------------------ */
/*  Two-qubit gates                                                    */
/* ------------------------------------------------------------------ */

/**
 * @brief Adds a controlled-X (CNOT) gate.
 *
 * @param[in] ir  The circuit IR handle.
 * @param[in] qc  Control qubit index.
 * @param[in] qt  Target qubit index.
 * @return 0 on success, non-zero on failure.
 */
int medusa_add_cx(circuit_ir_t *ir, uint32_t qc, uint32_t qt);

/**
 * @brief Adds a controlled-Z gate.
 *
 * @param[in] ir  The circuit IR handle.
 * @param[in] qc  Control qubit index.
 * @param[in] qt  Target qubit index.
 * @return 0 on success, non-zero on failure.
 */
int medusa_add_cz(circuit_ir_t *ir, uint32_t qc, uint32_t qt);

/* ------------------------------------------------------------------ */
/*  Three-qubit gates                                                  */
/* ------------------------------------------------------------------ */

/**
 * @brief Adds a Toffoli (CCX) gate.
 *
 * @param[in] ir   The circuit IR handle.
 * @param[in] qc1  First control qubit index.
 * @param[in] qc2  Second control qubit index.
 * @param[in] qt   Target qubit index.
 * @return 0 on success, non-zero on failure.
 */
int medusa_add_ccx(circuit_ir_t *ir, uint32_t qc1, uint32_t qc2, uint32_t qt);

/* ------------------------------------------------------------------ */
/*  Multi-qubit gates                                                  */
/* ------------------------------------------------------------------ */

/**
 * @brief Adds a multi-controlled X (MCX) gate.
 *
 * The array is copied internally; the caller retains ownership.
 *
 * @param[in] ir        The circuit IR handle.
 * @param[in] qubits    Array of qubit indices: [target, ctrl1, ctrl2, ...].
 * @param[in] n_qubits  Total number of entries in @p qubits.
 * @return 0 on success, non-zero on failure.
 */
int medusa_add_mcx(circuit_ir_t *ir, uint32_t *qubits, uint32_t n_qubits);

/* ------------------------------------------------------------------ */
/*  Measurement                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief Adds a measurement instruction.
 *
 * @param[in] ir  The circuit IR handle.
 * @param[in] qt  Qubit to measure.
 * @param[in] ct  Classical bit index to store the result.
 * @return 0 on success, non-zero on failure.
 */
int medusa_add_measure(circuit_ir_t *ir, uint32_t qt, uint32_t ct);

/* ------------------------------------------------------------------ */
/*  Assertions                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief Adds an assertion that a given quantum state has a certain probability.
 *
 * @note The probability can differ by a small epsilon.
 *
 * @param[in] ir  The circuit IR handle.
 * @param[in] state_str  String representation of the quantum state (e.g. "0101").
 * @param[in] prob_threshold  The minimum probability threshold for the assertion.
 * @return 0 on success, non-zero on failure.
 */
int medusa_add_assert_eq(circuit_ir_t *ir, const char *state_str, double prob_threshold);

/**
 * @brief Adds an assertion that a given set of qubits is in a superposition state.
 *
 * @param[in] ir  The circuit IR handle.
 * @param[in] qubits    Array of qubit indices to be asserted in superposition.
 * @param[in] n_qubits  Number of qubits in the array.
 * @return 0 on success, non-zero on failure.
 */
int medusa_add_assert_sup(circuit_ir_t *ir, const uint32_t *qubits, uint32_t n_qubits);

/**
 * @brief Adds an assertion that a given set of qubits is interaction-connected.
 *
 * @param[in] ir  The circuit IR handle.
 * @param[in] qubits    Array of qubit indices to be asserted as interaction-connected.
 * @param[in] n_qubits  Number of qubits in the array.
 * @return 0 on success, non-zero on failure.
 */
int medusa_add_assert_interact(circuit_ir_t *ir, const uint32_t *qubits, uint32_t n_qubits);

/**
 * @brief Adds an assertion that a given set of qubits is in an entangled state.
 *
 * @param[in] ir  The circuit IR handle.
 * @param[in] qubits    Array of qubit indices to be asserted in entanglement.
 * @param[in] n_qubits  Number of qubits in the array.
 * @return 0 on success, non-zero on failure.
 */
int medusa_add_assert_ent(circuit_ir_t *ir, const uint32_t *qubits, uint32_t n_qubits);

/* ------------------------------------------------------------------ */
/*  Simulation                                                         */
/* ------------------------------------------------------------------ */

/**
 * @brief Simulates a quantum circuit from a given QASM file.
 *
 * @param[in] filename Path to the QASM file.
 * @param[in][in] symbolic Whether to simulate loops symbolically.
 * @param[out] mtbdd Resulting MTBDD representation of the simulated circuit.
 * @return 0 on success, non-zero on failure.
 */
int medusa_simulate_file(const char *filename, int symbolic, MTBDD *mtbdd);

/**
 * @brief Simulates a quantum circuit built via the circuit IR handle API.
 *
 * @param[in] ir         The circuit IR handle populated with gates.
 * @param[in] symbolic   Whether to simulate loops symbolically.
 * @param[out] mtbdd Resulting MTBDD representation of the simulated circuit.
 * @return 0 on success, non-zero on failure.
 */
int medusa_simulate_circuit(circuit_ir_t *ir, int symbolic, MTBDD *mtbdd);

/* ------------------------------------------------------------------ */
/*  Results                                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief Retrieves the measurement counts from the simulation.
 *
 * @param[in] mtbdd MTBDD representing the final state of the quantum circuit.
 * @param[in] num_qubits Number of qubits in the circuit.
 * @param[out] indices NULL-terminated array of qubit state strings (e.g. "0101").
 * @param[out] probs NULL-terminated array of corresponding probabilities. Index i in probs corresponds to index i in indices.
 * @return 0 on success, non-zero on failure.
 */
int medusa_get_counts(MTBDD mtbdd, int num_qubits, char **indices[], double **probs);

/**
 * @brief Frees memory allocated by ::medusa_get_counts().
 *
 * @param[in] indices NULL-terminated array of qubit state strings to be freed.
 * @param[in] probs NULL-terminated array of probabilities to be freed.
 */
void medusa_free_counts(char **indices, double *probs);

#endif /* _MEDUSA_H_ */
