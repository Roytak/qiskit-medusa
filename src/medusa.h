
#ifndef _MEDUSA_H_
#define _MEDUSA_H_

#include "sim.h"

typedef struct simulator_ctx {
    sim_info_t sim_info;
    MTBDD circuit;
    sim_flags_t sim_flags;
} simulator_ctx_t;

/**
 * @brief Initializes the Medusa simulator.
 */
void medusa_init(void);

/**
 * @brief Destroys the Medusa simulator and frees associated resources.
 */
void medusa_destroy(void);

/**
 * @brief Initializes a new simulator context.
 *
 * @return Pointer to the newly created simulator context, or NULL on failure.
 */
simulator_ctx_t *medusa_simulator_ctx_init(void);

/**
 * @brief Simulates a quantum circuit from a given QASM file.
 *
 * @param ctx Pointer to the simulator context.
 * @param filename Path to the QASM file.
 * @return 0 on success, non-zero on failure.
 */
int medusa_simulate_file(simulator_ctx_t *ctx, const char *filename);

/**
 * @brief Retrieves the measurement counts from the simulation.
 *
 * @param ctx Pointer to the simulator context.
 * @param num_qubits Number of qubits in the circuit.
 * @param indices NULL-terminated array of qubit state strings (e.g. "0101").
 * @param probs NULL-terminated array of corresponding probabilities. Index i in probs corresponds to index i in indices.
 * @return 0 on success, non-zero on failure.
 */
int medusa_get_counts(simulator_ctx_t *ctx, int num_qubits, char **indices[], double **probs);

/**
 * @brief Frees memory allocated by ::medusa_get_counts().
 *
 * @param indices NULL-terminated array of qubit state strings to be freed.
 * @param probs NULL-terminated array of probabilities to be freed.
 */
void medusa_free_counts(char **indices, double *probs);

#endif /* _MEDUSA_H_ */
