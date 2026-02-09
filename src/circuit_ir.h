/**
 * @file circuit_ir.h
 * @brief Intermediate representation for a quantum circuit as a sequential list of instructions.
 *
 * The circuit IR decouples parsing from simulation: a parser populates the IR,
 * and the simulator executes it.  New gate types can be added by extending
 * `gate_kind_t` and the corresponding union arm in `gate_instr_t`.
 */

#ifndef CIRCUIT_IR_H
#define CIRCUIT_IR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Gate kinds                                                         */
/* ------------------------------------------------------------------ */

/// Every supported instruction type.  Extend this enum when adding a gate.
typedef enum {
    /* Single-qubit gates */
    GATE_X,
    GATE_Y,
    GATE_Z,
    GATE_H,
    GATE_S,
    GATE_T,
    GATE_RX_PIHALF,
    GATE_RY_PIHALF,

    /* Two-qubit gates */
    GATE_CX,
    GATE_CZ,

    /* Three-qubit gates */
    GATE_CCX,

    /* Multi-qubit gates */
    GATE_MCX,

    /* Measurement */
    GATE_MEASURE,

    /* Loop control */
    GATE_LOOP_START,
    GATE_LOOP_END,
} gate_kind_t;

/* ------------------------------------------------------------------ */
/*  Instruction                                                        */
/* ------------------------------------------------------------------ */

/// A single instruction in the circuit IR.
typedef struct gate_instr {
    gate_kind_t kind;

    union {
        /** Single-qubit gate (X, Y, Z, H, S, T, RX(pi/2), RY(pi/2)) */
        struct {
            uint32_t qt;          ///< target qubit
        } single;

        /** Controlled gate with one control (CX, CZ) */
        struct {
            uint32_t qc;          ///< control qubit
            uint32_t qt;          ///< target qubit
        } controlled;

        /** Toffoli gate (CCX) */
        struct {
            uint32_t qc1;         ///< first control qubit
            uint32_t qc2;         ///< second control qubit
            uint32_t qt;          ///< target qubit
        } toffoli;

        /** Multi-controlled X gate (MCX) – variable number of qubits */
        struct {
            uint32_t *qubits;     ///< heap-allocated array [target, ctrl1, ctrl2, ...]
            uint32_t  n_qubits;   ///< total entries in `qubits`
        } multi;

        /** Measurement */
        struct {
            uint32_t qt;          ///< qubit to measure
            uint32_t ct;          ///< classical bit index
        } measure;

        /** Loop start (for … { ) */
        struct {
            uint64_t iters;       ///< number of iterations
            size_t   body_end;    ///< index of the matching GATE_LOOP_END
        } loop_start;

        /** Loop end ( } ) */
        struct {
            size_t   body_start;  ///< index of the matching GATE_LOOP_START
        } loop_end;
    } p;                           ///< instruction parameters
} gate_instr_t;

/* ------------------------------------------------------------------ */
/*  Circuit IR (the full instruction list)                             */
/* ------------------------------------------------------------------ */

/// Initial capacity for the instruction array (doubles on resize).
#define CIRCUIT_IR_INIT_CAP 256

/// A parsed quantum circuit stored as a flat array of instructions.
typedef struct circuit_ir {
    gate_instr_t *instrs;         ///< dynamic array of instructions
    size_t        len;            ///< number of instructions stored
    size_t        cap;            ///< allocated capacity

    uint32_t      n_qubits;       ///< number of qubits
    int           n_bits;         ///< number of classical bits (0 if none)
    int          *bits_to_measure; ///< qubit → classical bit mapping (NULL if none)
    bool          has_measure;    ///< true if any measure instruction was added
} circuit_ir_t;

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

/**
 * Allocates and returns a new, empty circuit IR.
 */
circuit_ir_t *circuit_ir_create(void);

/**
 * Frees the circuit IR and all owned memory (including MCX qubit arrays).
 */
void circuit_ir_destroy(circuit_ir_t *ir);

/* ------------------------------------------------------------------ */
/*  Instruction-adding helpers (one per gate kind)                     */
/* ------------------------------------------------------------------ */

/** @name Single-qubit gates */
///@{
void circuit_ir_add_x        (circuit_ir_t *ir, uint32_t qt);
void circuit_ir_add_y        (circuit_ir_t *ir, uint32_t qt);
void circuit_ir_add_z        (circuit_ir_t *ir, uint32_t qt);
void circuit_ir_add_h        (circuit_ir_t *ir, uint32_t qt);
void circuit_ir_add_s        (circuit_ir_t *ir, uint32_t qt);
void circuit_ir_add_t        (circuit_ir_t *ir, uint32_t qt);
void circuit_ir_add_rx_pihalf(circuit_ir_t *ir, uint32_t qt);
void circuit_ir_add_ry_pihalf(circuit_ir_t *ir, uint32_t qt);
///@}

/** @name Two-qubit gates */
///@{
void circuit_ir_add_cx(circuit_ir_t *ir, uint32_t qc, uint32_t qt);
void circuit_ir_add_cz(circuit_ir_t *ir, uint32_t qc, uint32_t qt);
///@}

/** @name Three-qubit gates */
///@{
void circuit_ir_add_ccx(circuit_ir_t *ir, uint32_t qc1, uint32_t qc2, uint32_t qt);
///@}

/**
 * Adds an MCX gate.  `qubits` is a heap-allocated array of length `n_qubits`
 * whose ownership is transferred to the IR (freed by `circuit_ir_destroy`).
 * The first element is the target qubit; the rest are controls.
 */
void circuit_ir_add_mcx(circuit_ir_t *ir, uint32_t *qubits, uint32_t n_qubits);

/**
 * Adds a measure instruction (qubit qt → classical bit ct).
 */
void circuit_ir_add_measure(circuit_ir_t *ir, uint32_t qt, uint32_t ct);

/**
 * Adds a GATE_LOOP_START instruction and returns its index in the instruction
 * array so the caller can later patch `body_end` when GATE_LOOP_END is added.
 */
size_t circuit_ir_add_loop_start(circuit_ir_t *ir, uint64_t iters);

/**
 * Adds a GATE_LOOP_END instruction and patches the corresponding
 * GATE_LOOP_START at `loop_start_idx` with the correct `body_end`.
 */
void circuit_ir_add_loop_end(circuit_ir_t *ir, size_t loop_start_idx);

/* ------------------------------------------------------------------ */
/*  Qubit / bit register helpers                                       */
/* ------------------------------------------------------------------ */

/**
 * Sets the number of qubits in the circuit.
 */
void circuit_ir_set_qubits(circuit_ir_t *ir, uint32_t n);

/**
 * Sets the number of classical bits and allocates `bits_to_measure`.
 */
void circuit_ir_set_bits(circuit_ir_t *ir, uint32_t n);

#endif /* CIRCUIT_IR_H */
