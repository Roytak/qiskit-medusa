/**
 * @file sim.h
 * @brief Module performing quantum circuit simulation
 */

#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <stdbool.h>
#include <time.h>

#include "assertions.h"
#include "mtbdd.h"
#include "circuit_ir.h"

/// Eps for checking if probability is > 0 & < 1
#define EPSILON 0.001

/// Flags to run the simulation with
typedef struct sim_flags {
    bool opt_symb;   /// If true, loops will be simulated symbolically
    bool opt_info;   /// If true, time statistics will be collected for loop simulation
} sim_flags_t;

/// Additional simulation info (set during the simulation)
typedef struct sim_info {
    int n_qubits;         /// Number of qubits in the circuit
    bool is_measure;      /// True if some measure operation is encountered
    int *bits_to_measure; /// Array for storing the qubits that are to be measured
    size_t n_loops;       /// Total number of loops in the circuit
    size_t t_len;         /// Lenght of both time arrays
    double *t_el_loop;    /// Array for the total elapsed time for each loop
    double *t_el_eval;    /// Array for the eval time for each loop (for symbolic sim only)
} sim_info_t;

/**
 * Initializes the structure for the data collected during the simulation
 */
void init_sim_info(sim_info_t *i);

/**
 * Parses a QASM file into a circuit intermediate representation.
 *
 * @param in input QASM file
 *
 * @return A newly allocated circuit IR, or NULL on parse failure.
 *         The caller is responsible for calling circuit_ir_destroy().
 */
circuit_ir_t *parse_qasm(FILE *in);

/**
 * Simulates a parsed circuit IR on an MTBDD backend.
 *
 * @param ir          The parsed circuit (must not be NULL)
 * @param is_symbolic If true, loops will be simulated symbolically
 * @param circ        Pointer to MTBDD where the resulting circuit will be stored
 *
 * @return true if the circuit has been properly simulated
 */
bool simulate_ir(const circuit_ir_t *ir, int is_symbolic, MTBDD *circ);

/**
 * Convenience wrapper: parses a QASM file and simulates it in one call.
 * Equivalent to parse_qasm() followed by simulate_ir().
 *
 * @param in input QASM file
 * @param is_symbolic If true, loops will be simulated symbolically
 * @param circ Pointer to MTBDD where the resulting circuit will be stored
 *
 * @return true if the circuit has been properly initialized and simulated
 */
bool sim_file(FILE *in, int is_symbolic, MTBDD *circ);

/**
 * Measures all bits in the given array (compatible only with measurement at the end of the circuit)
 *
 * @param samples the total number of samples
 *
 * @param output stream for the output of results
 *
 * @param circ MTBDD representation of the end circuit state
 *
 * @param n number of qubits in the circuit
 *
 * @param bits_to_measure array for storing the qubits that are to be measured
 *
 */
void measure_all(unsigned long samples, FILE *output, MTBDD circ, int n, int *bits_to_measure);

/**
 * Calculates the time elapsed between two timestamps in seconds
 */
static inline double get_time_el(struct timespec start, struct timespec fin)
{
    return fin.tv_sec - start.tv_sec + (fin.tv_nsec - start.tv_nsec) * 1.0e-9;
}

#endif
/* end of "sim.h" */
