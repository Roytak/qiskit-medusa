/**
 * @file circuit_ir.c
 * @brief Implementation of the circuit intermediate representation.
 */

#include <stdlib.h>
#include <string.h>
#include "circuit_ir.h"
#include "error.h"

/* ------------------------------------------------------------------ */
/*  Internal: ensure there is room for at least one more instruction   */
/* ------------------------------------------------------------------ */

static void ensure_capacity(circuit_ir_t *ir)
{
    if (ir->len < ir->cap) {
        return;
    }
    size_t new_cap = (ir->cap == 0) ? CIRCUIT_IR_INIT_CAP : ir->cap * 2;
    ir->instrs = my_realloc(ir->instrs, new_cap * sizeof(gate_instr_t));
    ir->cap = new_cap;
}

/**
 * Appends a pre-filled instruction and returns its index.
 */
static size_t push_instr(circuit_ir_t *ir, gate_instr_t instr)
{
    ensure_capacity(ir);
    size_t idx = ir->len;
    ir->instrs[idx] = instr;
    ir->len++;
    return idx;
}

/**
 * Records that qubits `q1` and `q2` interact (i.e. there is a gate that has them both as operands).
 */
static void add_interaction2(circuit_ir_t *ir, uint32_t q1, uint32_t q2)
{
    /* Ensure qubit_interactions is allocated and large enough */
    if (!ir->qubit_interactions) {
        ir->qubit_interactions = my_malloc(ir->n_qubits * sizeof(uint32_t*));
        for (uint32_t i = 0; i < ir->n_qubits; i++) {
            ir->qubit_interactions[i] = my_malloc(ir->n_qubits * sizeof(uint32_t));
        }
    }

    /* Add q2 to q1's interaction list */
    ir->qubit_interactions[q1][q2] = 1;

    /* Add q1 to q2's interaction list */
    ir->qubit_interactions[q2][q1] = 1;
}

/**
 * Records that qubits `q1`, `q2`, and `q3` interact with each other (i.e. there is a gate that has them all as operands).
 */
static void add_interaction3(circuit_ir_t *ir, uint32_t q1, uint32_t q2, uint32_t q3)
{
    /* add interactions for all pairs among q1, q2, q3 */
    add_interaction2(ir, q1, q2);
    add_interaction2(ir, q1, q3);
    add_interaction2(ir, q2, q3);
}

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

circuit_ir_t *circuit_ir_create(void)
{
    circuit_ir_t *ir = my_malloc(sizeof(circuit_ir_t));
    ir->instrs = NULL;
    ir->len = 0;
    ir->cap = 0;
    ir->n_qubits = 0;
    ir->n_bits = 0;
    ir->bits_to_measure = NULL;
    ir->has_measure = false;
    ir->qubit_interactions = NULL;
    ir->errs = NULL;
    ir->err_count = 0;
    return ir;
}

void circuit_ir_destroy(circuit_ir_t *ir)
{
    if (!ir) {
        return;
    }
    /* Free heap-allocated qubit arrays owned by MCX instructions */
    for (size_t i = 0; i < ir->len; i++) {
        if (ir->instrs[i].kind == GATE_MCX) {
            free(ir->instrs[i].p.multi.qubits);
        } else if (ir->instrs[i].kind == GATE_ASSERT_EQ) {
            free(ir->instrs[i].p.assert.state_str);
        } else if (ir->instrs[i].kind == GATE_ASSERT_SUP || ir->instrs[i].kind == GATE_ASSERT_INTERACT || ir->instrs[i].kind == GATE_ASSERT_ENT) {
            free(ir->instrs[i].p.assert.qubits);
        }
    }
    /* Free qubit interaction arrays */
    if (ir->qubit_interactions) {
        for (uint32_t i = 0; i < ir->n_qubits; i++) {
            free(ir->qubit_interactions[i]);
        }
        free(ir->qubit_interactions);
    }
    free(ir->instrs);
    free(ir->bits_to_measure);
    free(ir->errs);
    free(ir);
}

/* ------------------------------------------------------------------ */
/*  Single-qubit gate helpers                                          */
/* ------------------------------------------------------------------ */

static void add_single(circuit_ir_t *ir, gate_kind_t kind, uint32_t qt)
{
    gate_instr_t instr = { .kind = kind, .p.single = { .qt = qt } };
    push_instr(ir, instr);
}

void circuit_ir_add_x        (circuit_ir_t *ir, uint32_t qt) { add_single(ir, GATE_X,         qt); }
void circuit_ir_add_y        (circuit_ir_t *ir, uint32_t qt) { add_single(ir, GATE_Y,         qt); }
void circuit_ir_add_z        (circuit_ir_t *ir, uint32_t qt) { add_single(ir, GATE_Z,         qt); }
void circuit_ir_add_h        (circuit_ir_t *ir, uint32_t qt) { add_single(ir, GATE_H,         qt); }
void circuit_ir_add_s        (circuit_ir_t *ir, uint32_t qt) { add_single(ir, GATE_S,         qt); }
void circuit_ir_add_t        (circuit_ir_t *ir, uint32_t qt) { add_single(ir, GATE_T,         qt); }
void circuit_ir_add_rx_pihalf(circuit_ir_t *ir, uint32_t qt) { add_single(ir, GATE_RX_PIHALF, qt); }
void circuit_ir_add_ry_pihalf(circuit_ir_t *ir, uint32_t qt) { add_single(ir, GATE_RY_PIHALF, qt); }

/* ------------------------------------------------------------------ */
/*  Two-qubit gate helpers                                             */
/* ------------------------------------------------------------------ */

void circuit_ir_add_cx(circuit_ir_t *ir, uint32_t qc, uint32_t qt)
{
    gate_instr_t instr = { .kind = GATE_CX, .p.controlled = { .qc = qc, .qt = qt } };
    push_instr(ir, instr);
    add_interaction2(ir, qc, qt);
}

void circuit_ir_add_cz(circuit_ir_t *ir, uint32_t qc, uint32_t qt)
{
    gate_instr_t instr = { .kind = GATE_CZ, .p.controlled = { .qc = qc, .qt = qt } };
    push_instr(ir, instr);
    add_interaction2(ir, qc, qt);
}

/* ------------------------------------------------------------------ */
/*  Three-qubit gate helper                                            */
/* ------------------------------------------------------------------ */

void circuit_ir_add_ccx(circuit_ir_t *ir, uint32_t qc1, uint32_t qc2, uint32_t qt)
{
    gate_instr_t instr = {
        .kind = GATE_CCX,
        .p.toffoli = { .qc1 = qc1, .qc2 = qc2, .qt = qt }
    };
    push_instr(ir, instr);
    add_interaction3(ir, qc1, qc2, qt);
}

/* ------------------------------------------------------------------ */
/*  Multi-qubit gate (MCX)                                             */
/* ------------------------------------------------------------------ */

void circuit_ir_add_mcx(circuit_ir_t *ir, uint32_t *qubits, uint32_t n_qubits)
{
    gate_instr_t instr = {
        .kind = GATE_MCX,
        .p.multi = { .qubits = qubits, .n_qubits = n_qubits }
    };
    push_instr(ir, instr);
    /* Add interactions for all pairs of qubits in the MCX */
    for (uint32_t i = 0; i < n_qubits; i++) {
        for (uint32_t j = i + 1; j < n_qubits; j++) {
            add_interaction2(ir, qubits[i], qubits[j]);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Measurement                                                        */
/* ------------------------------------------------------------------ */

void circuit_ir_add_measure(circuit_ir_t *ir, uint32_t qt, uint32_t ct)
{
    gate_instr_t instr = {
        .kind = GATE_MEASURE,
        .p.measure = { .qt = qt, .ct = ct }
    };
    push_instr(ir, instr);
    ir->has_measure = true;
    if (ir->bits_to_measure) {
        ir->bits_to_measure[qt] = (int)ct;
    }
}

/* ------------------------------------------------------------------ */
/*  Assertions                                                        */
/* ------------------------------------------------------------------ */

void
circuit_ir_add_assert_eq(circuit_ir_t *ir, char *state_str, double prob_threshold)
{
    gate_instr_t instr = {
        .kind = GATE_ASSERT_EQ,
        .p.assert = { .prob = prob_threshold, .state_str = state_str }
    };
    push_instr(ir, instr);
}

void
circuit_ir_add_assert_sup(circuit_ir_t *ir, uint32_t *qubits, uint32_t n_qubits)
{
    gate_instr_t instr = {
        .kind = GATE_ASSERT_SUP,
        .p.assert = { .qubits = qubits, .n_qubits = n_qubits }
    };
    push_instr(ir, instr);
}

void
circuit_ir_add_assert_interact(circuit_ir_t *ir, uint32_t *qubits, uint32_t n_qubits)
{
    gate_instr_t instr = {
        .kind = GATE_ASSERT_INTERACT,
        .p.assert = { .qubits = qubits, .n_qubits = n_qubits }
    };
    push_instr(ir, instr);
}

void
circuit_ir_add_assert_ent(circuit_ir_t *ir, uint32_t *qubits, uint32_t n_qubits)
{
    gate_instr_t instr = {
        .kind = GATE_ASSERT_ENT,
        .p.assert = { .qubits = qubits, .n_qubits = n_qubits }
    };
    push_instr(ir, instr);
}

/* ------------------------------------------------------------------ */
/*  Loop control                                                       */
/* ------------------------------------------------------------------ */

size_t circuit_ir_add_loop_start(circuit_ir_t *ir, uint64_t iters)
{
    gate_instr_t instr = {
        .kind = GATE_LOOP_START,
        .p.loop_start = { .iters = iters, .body_end = 0 }  /* patched later */
    };
    return push_instr(ir, instr);
}

void circuit_ir_add_loop_end(circuit_ir_t *ir, size_t loop_start_idx)
{
    size_t end_idx = ir->len;  /* index the new LOOP_END will occupy */

    gate_instr_t instr = {
        .kind = GATE_LOOP_END,
        .p.loop_end = { .body_start = loop_start_idx }
    };
    push_instr(ir, instr);

    /* Patch the matching LOOP_START */
    ir->instrs[loop_start_idx].p.loop_start.body_end = end_idx;
}

/* ------------------------------------------------------------------ */
/*  Qubit / bit register helpers                                       */
/* ------------------------------------------------------------------ */

/// Constant for index into classical bit register if the qubit shouldn't be measured
#define Q_NOT_MEASURED (-1)

void circuit_ir_set_qubits(circuit_ir_t *ir, uint32_t n)
{
    ir->n_qubits = n;
}

void circuit_ir_set_bits(circuit_ir_t *ir, uint32_t n)
{
    ir->n_bits = (int)n;
    ir->bits_to_measure = my_malloc(n * sizeof(int));
    for (uint32_t i = 0; i < n; i++) {
        ir->bits_to_measure[i] = Q_NOT_MEASURED;
    }
}

/* end of "circuit_ir.c" */
