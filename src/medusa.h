
#ifndef _MEDUSA_H_
#define _MEDUSA_H_

/**
 * @brief Initializes the Medusa simulator.
 */
void medusa_init(void);

/**
 * @brief Destroys the Medusa simulator and frees associated resources.
 */
void medusa_destroy(void);

/**
 * @brief Simulates a quantum circuit from a given QASM file.
 *
 * @param filename Path to the QASM file.
 * @param[in] symbolic Whether to simulate loops symbolically.
 * @param[out] mtbdd Resulting MTBDD representation of the simulated circuit.
 * @return 0 on success, non-zero on failure.
 */
int medusa_simulate_file(const char *filename, int symbolic, MTBDD *mtbdd);

/**
 * @brief Retrieves the measurement counts from the simulation.
 *
 * @param[in] mtbdd MTBDD representing the final state of the quantum circuit.
 * @param num_qubits Number of qubits in the circuit.
 * @param indices NULL-terminated array of qubit state strings (e.g. "0101").
 * @param probs NULL-terminated array of corresponding probabilities. Index i in probs corresponds to index i in indices.
 * @return 0 on success, non-zero on failure.
 */
int medusa_get_counts(MTBDD mtbdd, int num_qubits, char **indices[], double **probs);

/**
 * @brief Frees memory allocated by ::medusa_get_counts().
 *
 * @param indices NULL-terminated array of qubit state strings to be freed.
 * @param probs NULL-terminated array of probabilities to be freed.
 */
void medusa_free_counts(char **indices, double *probs);

#endif /* _MEDUSA_H_ */
