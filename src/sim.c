#include <ctype.h>  // For isspace(), isdigit()
#include <string.h>
#include <math.h>
#include <errno.h>
#include <assert.h>
#include "sim.h"
#include "circuit_ir.h"
#include "gates.h"
#include "gates_symb.h"
#include "mtbdd_symb_val.h"
#include "symb_utils.h"
#include "qparam.h"
#include "htab.h"
#include "error.h"
#include "medusa.h"

/// Max. supported length of the string with classical bit register identifier (includes '\0')
#define BIT_REG_ID_MAX_LEN (30+1)
/// Max. supported length of the string with the qasm command (set to identifier length because of OpenQASM 3 measurement syntax)
#define CMD_MAX_LEN BIT_REG_ID_MAX_LEN
/// Max. number of characters in a parsed number
#define NUM_MAX_LEN 25
/// Constant for no other possible end character for 'parse_num()'
#define NO_ALT_END -2
/// Constant for index into classical bit register used for measurement results if the qubit shouldn't be measured
#define Q_NOT_MEASURED -1
/// Eps for checking if probability is > 0 & < 1
#define EPSILON 0.001
/// Size increment for time arrays resize in sim_info
#define TIMES_RESIZE_COEF 10

void init_sim_info(sim_info_t *i)
{
    i->n_qubits = 0;
    i->is_measure = false;
    i->bits_to_measure = NULL;
    i->n_loops = 0;
    i->t_len = 0;
    i->t_el_loop = NULL;
    i->t_el_eval = NULL;
}

/**
 * Resizes both the arrays used for time tracking to the new size (or allocates memory if empty)
 */
static void sim_info_times_addsize(sim_info_t *i, int inc)
{
    size_t size = inc;
    if (i->t_len == 0) {
        i->t_el_loop = my_malloc(sizeof(double) * size);
        i->t_el_eval = my_malloc(sizeof(double) * size);
    }
    else {
        size += inc;
        i->t_el_loop = my_realloc(i->t_el_loop, sizeof(double) * size);
        i->t_el_eval = my_realloc(i->t_el_loop, sizeof(double) * size);
    }
    i->t_len = size;
}

/* ================================================================== */
/*  QASM Parsing Helpers                                               */
/* ================================================================== */

/**
 * Function for number parsing from the input file (reads the number from the input until the end character is encountered)
 * Checks for two possible end characters. If only one character should be checked agains, set alt_end to NO_ALT_END.
 */
static long long parse_num(FILE *in, char end, char alt_end)
{
    int c = fgetc(in);
    char num[NUM_MAX_LEN] = {0};
    long long n;

    // Skip leading whitespace
    while (isspace(c)) {
        c = fgetc(in);
    }

    // Load number to string
    while ( c != end && c != alt_end) {
        if (c == EOF) {
            error_exit("Invalid format - reached an unexpected end of file when converting a number.\n");
        }
        else if (!isdigit(c) && c != '-') {
            // Check if isn't just trailing whitespace
            while (isspace(c)) {
                c = fgetc(in);
            }
            if (c == end || c == alt_end) {
                break;
            }
            else {
                error_exit("Invalid format - not a valid number (a non-digit character '%c' encountered while parsing a number).\n", c);
            }
        }
        else if (strlen(num) + 1 < NUM_MAX_LEN) {
            int *temp = &c;
            strncat(num, (char*)temp, 1);
        }
        else {
            error_exit("Invalid format - not a valid number (too many digits).\n");
        }
        c = fgetc(in);
    }

    // Convert to integer value
    char *ptr;
    errno = 0;
    n = strtoll(num, &ptr, 10);
    if (num == ptr || errno != 0) {
        error_exit("Invalid format - not a valid number.\n");
    }
    return n;
}

/**
 * Function for getting the next qubit index for the command on the given line.
 */
static uint32_t get_q_num(FILE *in)
{
    int c;
    long long n;

    while ((c = fgetc(in)) != '[') {
        if (c == EOF) {
            error_exit("Invalid format - reached an unexpected end of file (expected a qubit index).\n");
        }
    }

    n = parse_num(in, ']', NO_ALT_END);
    if (n > UINT32_MAX || n < 0) {
        error_exit("Invalid format - not a valid qubit identifier.\n");
    }

    return ((uint32_t)n);
}

/**
 * Returns the number of iterations, should be called when a for loop is encountered
 */
static uint64_t get_iters(FILE *in)
{
    int c;
    long long start, end;
    long long step = 1;
    uint64_t iters;

    while ((c = fgetc(in)) != '[') {
        if (c == EOF) {
            error_exit("Invalid format - reached an unexpected end of file (expected a number of loop iterations).\n");
        }
    }

    start = parse_num(in, ':', NO_ALT_END);
    end = parse_num(in, ']', ':');
    if (fseek(in, -1, SEEK_CUR) != 0) {
        error_exit("Error has occured when parsing loop parameters (fseek error). Circuits with loops cannot be simulated from stdin.\n");
    }
    if ((c = fgetc(in)) == ':') {
        step = end;
        end = parse_num(in, ']', NO_ALT_END);
    }

    // Note: expects 64bit long long
    if (step == 0) {
        error_exit("Invalid number of loop iterations - step must be non-zero.\n");
    }
    else if ((end == LLONG_MAX && start == LLONG_MIN) || (end == LLONG_MIN && start == LLONG_MAX)) {
        error_exit("Invalid number of loop iterations - overflow detected.\n");
    }
    else if ((end + 1 - start) % step != 0) {
        error_exit("Invalid number of loop iterations - not an integer.\n");
    }
    else if ((end < start && step > 0) || (end > start && step < 0)) {
        error_exit("Invalid number of loop iterations.\n");
    }

    iters = (end + 1 - start) / step;

    return ((uint64_t) iters);
}

/**
 * Skips a one line comment (checks if the given char doesn't mark a start of a comment)
 *
 * @return Returns 1 if comment has been skipped, -1 if EOF was reached, 0 if there is no comment
 */
static int skip_one_line_comments(char c, FILE *in)
{
    if (c == '/') {
        if ((c = fgetc(in)) == '/') {
            while ((c = fgetc(in)) != '\n') {
                if (c == EOF) {
                    return -1;
                }
            }
            return 1;
        }
        else {
            error_exit("Invalid command, expected a one-line comment.\n");
        }
    }
    return 0;
}

/* ================================================================== */
/*  parse_qasm  –  QASM file  →  circuit_ir_t                         */
/* ================================================================== */

circuit_ir_t *parse_qasm(FILE *in)
{
    int c;
    char cmd[CMD_MAX_LEN];
    char bit_reg[BIT_REG_ID_MAX_LEN] = {0};
    bool init = false;

    /* Index of the current (only) LOOP_START – used to patch LOOP_END.
     * Nested loops are not supported. */
    size_t loop_start_idx = 0;
    bool in_loop = false;

    circuit_ir_t *ir = circuit_ir_create();

    while ((c = fgetc(in)) != EOF) {
        for (int i = 0; i < CMD_MAX_LEN; i++) {
            cmd[i] = '\0';
        }

        while (isspace(c)) {
            c = fgetc(in);
        }

        if (c == EOF) {
            break;
        }

        /* Skip one-line comments */
        int comment_check = skip_one_line_comments(c, in);
        if (comment_check == -1) {
            break;
        }
        else if (comment_check == 1) {
            continue;
        }

        /* Load the command */
        do {
            if (c == EOF) {
                error_exit("Invalid format - reached an unexpected end of file when loading a command.\n");
            }
            int len = strlen(cmd);
            if (len + 1 < CMD_MAX_LEN) {
                cmd[len] = (char)c;
            }
            else {
                error_exit("Invalid command (command too long).\n");
            }
        } while (!isspace(c = fgetc(in)) && (c != '['));

        if (c == '[') {
            ungetc(c, in);
        }
        else if (c == EOF) {
            error_exit("Invalid format - reached an unexpected end of file immediately after a command.\n");
        }

        /* ---- Identify the command and populate the IR ---- */

        if (strcmp(cmd, "OPENQASM") == 0) {
            /* ignored – skip rest of line */
        }
        else if (strcmp(cmd, "include") == 0) {
            /* ignored – skip rest of line */
        }
        else if ((strcmp(cmd, "creg") == 0) || (strcmp(cmd, "bit") == 0)) {
            if (ir->n_bits != 0) {
                error_exit("Multiple bit register definitions encountered - currently not supported.\n");
            }

            uint32_t n = get_q_num(in);
            circuit_ir_set_bits(ir, n);

            if (ir->n_qubits != 0 && n != ir->n_qubits) {
                error_exit("Bit register size is different than the size of the qubit register - currently not supported.\n");
            }

            while (isspace(c = fgetc(in))) { }
            /* Save the register name to detect measurements */
            do {
                if (c == ';') {
                    break;
                }
                else if (c == EOF) {
                    error_exit("Invalid format - reached an unexpected end of file (bit register name expected).\n");
                }
                int len = strlen(bit_reg);
                if (len + 1 < BIT_REG_ID_MAX_LEN) {
                    bit_reg[len] = (char)c;
                }
                else {
                    error_exit("Invalid bit register identifier (identifier is too long, max supported length is %d).\n", BIT_REG_ID_MAX_LEN - 1);
                }
            } while (!isspace(c = fgetc(in)));
            if (c == ';') {
                continue; /* end of command */
            }
        }
        else if ((strcmp(cmd, "qreg") == 0) || (strcmp(cmd, "qubit") == 0)) {
            if (init) {
                error_exit("Multiple qubit register definitions encountered - currently not supported.\n");
            }

            uint32_t n = get_q_num(in);
            circuit_ir_set_qubits(ir, n);

            if (ir->n_bits != 0 && n != (uint32_t)ir->n_bits) {
                error_exit("Bit register size is different than the size of the qubit register - currently not supported.\n");
            }
            init = true;
        }
        else if (init) {
            /* ---- Loop control ---- */
            if (strcmp(cmd, "for") == 0) {
                if (in_loop) {
                    error_exit("Nested loops are not allowed, aborting.\n");
                }
                uint64_t iters = get_iters(in);
                if (iters == 0) {
                    /* Skip the loop body in the file */
                    while ((c = fgetc(in)) != '}') {
                        if (c == EOF || skip_one_line_comments(c, in) == -1) {
                            error_exit("Invalid format - reached an unexpected end of file (there is an unfinished loop).\n");
                        }
                    }
                    continue;
                }
                while ((c = fgetc(in)) != '{') {
                    if (c == EOF) {
                        error_exit("Invalid format - reached an unexpected end of file at the start of a loop.\n");
                    }
                }
                loop_start_idx = circuit_ir_add_loop_start(ir, iters);
                in_loop = true;
                continue; /* ';' not expected */
            }
            else if (strcmp(cmd, "}") == 0) {
                if (!in_loop) {
                    error_exit("Invalid loop syntax - reached an unexpected end of a loop.\n");
                }
                circuit_ir_add_loop_end(ir, loop_start_idx);
                in_loop = false;
                continue; /* ';' not expected */
            }
            /* ---- Measurement ---- */
            else if ((strcmp(cmd, "measure") == 0) || (strcmp(cmd, bit_reg) == 0)) {
                if (ir->bits_to_measure == NULL) {
                    error_exit("Measuring into an uninitialized bit register.\n");
                }
                uint32_t qt, ct;
                if (strcmp(cmd, "measure") == 0) {
                    qt = get_q_num(in);
                    ct = get_q_num(in);
                }
                else {
                    ct = get_q_num(in);
                    qt = get_q_num(in);
                }
                circuit_ir_add_measure(ir, qt, ct);
            }
            /* ---- Single-qubit gates ---- */
            else if (strcasecmp(cmd, "x") == 0) {
                circuit_ir_add_x(ir, get_q_num(in));
            }
            else if (strcasecmp(cmd, "y") == 0) {
                circuit_ir_add_y(ir, get_q_num(in));
            }
            else if (strcasecmp(cmd, "z") == 0) {
                circuit_ir_add_z(ir, get_q_num(in));
            }
            else if (strcasecmp(cmd, "h") == 0) {
                circuit_ir_add_h(ir, get_q_num(in));
            }
            else if (strcasecmp(cmd, "s") == 0) {
                circuit_ir_add_s(ir, get_q_num(in));
            }
            else if (strcasecmp(cmd, "t") == 0) {
                circuit_ir_add_t(ir, get_q_num(in));
            }
            else if (strcasecmp(cmd, "rx(pi/2)") == 0) {
                circuit_ir_add_rx_pihalf(ir, get_q_num(in));
            }
            else if (strcasecmp(cmd, "ry(pi/2)") == 0) {
                circuit_ir_add_ry_pihalf(ir, get_q_num(in));
            }
            /* ---- Two-qubit gates ---- */
            else if (strcasecmp(cmd, "cx") == 0) {
                uint32_t qc = get_q_num(in);
                uint32_t qt = get_q_num(in);
                circuit_ir_add_cx(ir, qc, qt);
            }
            else if (strcasecmp(cmd, "cz") == 0) {
                uint32_t qc = get_q_num(in);
                uint32_t qt = get_q_num(in);
                if (qc > qt) {
                    uint32_t temp = qt;
                    qt = qc;
                    qc = temp;
                }
                assert(qc != qt);
                circuit_ir_add_cz(ir, qc, qt);
            }
            /* ---- Three-qubit gates ---- */
            else if (strcasecmp(cmd, "ccx") == 0) {
                uint32_t qc1 = get_q_num(in);
                uint32_t qc2 = get_q_num(in);
                uint32_t qt  = get_q_num(in);
                circuit_ir_add_ccx(ir, qc1, qc2, qt);
            }
            /* ---- Multi-qubit gates ---- */
            else if (strcasecmp(cmd, "mcx") == 0) {
                /* Collect all qubit indices (target + controls) */
                uint32_t buf[64];
                uint32_t count = 0;

                while (true) {
                    if (count >= 64) {
                        error_exit("MCX gate with too many qubits (max 64).\n");
                    }
                    buf[count++] = get_q_num(in);
                    c = fgetc(in);
                    while (isspace(c)) {
                        c = fgetc(in);
                    }
                    if (c == ',') {
                        continue;
                    }
                    else if (c == ';') {
                        break;
                    }
                    else {
                        error_exit("Invalid 'mcx' gate syntax.\n");
                    }
                }

                /* The file lists qubits as: ctrl1, ctrl2, ..., target
                 * but qparam_list_insert_first reverses them, so the IR
                 * stores them in reversed order: [target, ..., ctrl2, ctrl1].
                 * Replicate that convention. */
                uint32_t *qubits = my_malloc(count * sizeof(uint32_t));
                for (uint32_t i = 0; i < count; i++) {
                    qubits[i] = buf[count - 1 - i];
                }
                circuit_ir_add_mcx(ir, qubits, count);
                continue; /* ';' already consumed */
            }
            else {
                error_exit("Invalid command '%s'.\n", cmd);
            }
        }
        else {
            error_exit("Circuit not initialized.\n");
        }

        /* Skip all remaining characters on the currently read line */
        while ((c = fgetc(in)) != ';') {
            if (c == EOF) {
                error_exit("Invalid format - reached an unexpected end of file (expected ';' to end the current line).\n");
            }
        }
    } /* while */

    if (!init) {
        circuit_ir_destroy(ir);
        return NULL;
    }

    return ir;
}

/* ================================================================== */
/*  simulate_ir  –  circuit_ir_t  →  MTBDD simulation                  */
/* ================================================================== */

/**
 * Dispatch a single gate instruction on the MTBDD.
 *
 * When `use_symb` is true the symbolic variant of the gate is called instead
 * (used for instructions inside a symbolically simulated loop body).
 */
static void dispatch_gate(const gate_instr_t *instr, bool use_symb,
                          MTBDD *circ, MTBDD *symb_val)
{
    switch (instr->kind) {
    /* -- single-qubit -------------------------------------------- */
    case GATE_X:
        use_symb ? gate_symb_x(symb_val, instr->p.single.qt)
                 : gate_x(circ, instr->p.single.qt);
        break;
    case GATE_Y:
        use_symb ? gate_symb_y(symb_val, instr->p.single.qt)
                 : gate_y(circ, instr->p.single.qt);
        break;
    case GATE_Z:
        use_symb ? gate_symb_z(symb_val, instr->p.single.qt)
                 : gate_z(circ, instr->p.single.qt);
        break;
    case GATE_H:
        use_symb ? gate_symb_h(symb_val, instr->p.single.qt)
                 : gate_h(circ, instr->p.single.qt);
        break;
    case GATE_S:
        use_symb ? gate_symb_s(symb_val, instr->p.single.qt)
                 : gate_s(circ, instr->p.single.qt);
        break;
    case GATE_T:
        use_symb ? gate_symb_t(symb_val, instr->p.single.qt)
                 : gate_t(circ, instr->p.single.qt);
        break;
    case GATE_RX_PIHALF:
        use_symb ? gate_symb_rx_pihalf(symb_val, instr->p.single.qt)
                 : gate_rx_pihalf(circ, instr->p.single.qt);
        break;
    case GATE_RY_PIHALF:
        use_symb ? gate_symb_ry_pihalf(symb_val, instr->p.single.qt)
                 : gate_ry_pihalf(circ, instr->p.single.qt);
        break;

    /* -- two-qubit ----------------------------------------------- */
    case GATE_CX:
        use_symb ? gate_symb_cnot(symb_val, instr->p.controlled.qt, instr->p.controlled.qc)
                 : gate_cnot(circ, instr->p.controlled.qt, instr->p.controlled.qc);
        break;
    case GATE_CZ:
        use_symb ? gate_symb_cz(symb_val, instr->p.controlled.qt, instr->p.controlled.qc)
                 : gate_cz(circ, instr->p.controlled.qt, instr->p.controlled.qc);
        break;

    /* -- three-qubit --------------------------------------------- */
    case GATE_CCX:
        use_symb ? gate_symb_toffoli(symb_val, instr->p.toffoli.qt, instr->p.toffoli.qc1, instr->p.toffoli.qc2)
                 : gate_toffoli(circ, instr->p.toffoli.qt, instr->p.toffoli.qc1, instr->p.toffoli.qc2);
        break;

    /* -- multi-qubit --------------------------------------------- */
    case GATE_MCX: {
        /* Rebuild a qparam_list from the stored qubit array.
         * The array is [target, ctrl1, ctrl2, ...] matching the reversed
         * insert_first order of the original code. */
        qparam_list_t *qp = qparam_list_create();
        /* Insert in forward order so the list ends up with first = last inserted = qubits[n-1] */
        for (uint32_t i = 0; i < instr->p.multi.n_qubits; i++) {
            qparam_list_insert_first(qp, instr->p.multi.qubits[i]);
        }
        use_symb ? gate_symb_mcx(symb_val, qp) : gate_mcx(circ, qp);
        qparam_list_del(qp);
        break;
    }

    /* -- these are handled by the loop/measure logic, not here --- */
    case GATE_MEASURE:
    case GATE_LOOP_START:
    case GATE_LOOP_END:
        break;
    }
}

bool simulate_ir(const circuit_ir_t *ir, int is_symbolic, MTBDD *circ)
{
    if (!ir || ir->n_qubits == 0) {
        return false;
    }

    /* Initialise the MTBDD circuit state */
    circuit_init(circ, ir->n_qubits);
    mtbdd_protect(circ);

    /* Simulation bookkeeping (timing, loop count) */
    sim_info_t info = {0};
    info.n_qubits = (int)ir->n_qubits;
    struct timespec t_loop_start, t_loop_finish, t_eval_start;

    /* Walk the instruction list */
    size_t pc = 0;
    while (pc < ir->len) {
        const gate_instr_t *instr = &ir->instrs[pc];

        switch (instr->kind) {

        /* ---- Loop start ---------------------------------------- */
        case GATE_LOOP_START: {
            uint64_t iters = instr->p.loop_start.iters;
            size_t body_start = pc + 1;
            size_t body_end   = instr->p.loop_start.body_end; /* index of GATE_LOOP_END */

            if (info.n_loops == info.t_len) {
                sim_info_times_addsize(&info, TIMES_RESIZE_COEF);
            }
            clock_gettime(CLOCK_MONOTONIC, &t_loop_start);

            if (!is_symbolic) {
                /* --- Non-symbolic: iterate the loop body N times --- */
                for (uint64_t it = 0; it < iters; it++) {
                    for (size_t j = body_start; j < body_end; j++) {
                        dispatch_gate(&ir->instrs[j], false, circ, NULL);
                    }
                }
                clock_gettime(CLOCK_MONOTONIC, &t_loop_finish);
                info.t_el_loop[info.n_loops] = get_time_el(t_loop_start, t_loop_finish);
                info.n_loops++;
            }
            else {
                /* --- Symbolic: refine until convergence ---------- */
                if (info.n_loops == 0) {
                    symexp_htab_init(1LL << 17);
                }
                mtbdd_symb_t symbc;
                symb_init(circ, &symbc);

                bool converged = false;
                while (!converged) {
                    /* Execute one symbolic iteration */
                    for (size_t j = body_start; j < body_end; j++) {
                        dispatch_gate(&ir->instrs[j], true, circ, &symbc.val);
                    }

                    rdata_t *rdata = rdata_create(symbc.vm);
                    if (symb_refine(&symbc, rdata)) {
                        converged = true;
                        clock_gettime(CLOCK_MONOTONIC, &t_eval_start);
                        symb_eval(circ, &symbc, iters, rdata);
                        clock_gettime(CLOCK_MONOTONIC, &t_loop_finish);
                        info.t_el_loop[info.n_loops] = get_time_el(t_loop_start, t_loop_finish);
                        info.t_el_eval[info.n_loops] = get_time_el(t_eval_start, t_loop_finish);
                        info.n_loops++;
                    }
                    rdata_delete(rdata);
                }
            }

            /* Skip past the loop body – continue after GATE_LOOP_END */
            pc = body_end + 1;
            continue; /* don't fall through to pc++ */
        }

        /* ---- Loop end (should only be reached via loop start) -- */
        case GATE_LOOP_END:
            /* Reached only if something is wrong; loops jump past this. */
            error_exit("Unexpected GATE_LOOP_END at instruction %zu.\n", pc);
            break;

        /* ---- Measurement (just recorded in the IR, nothing to execute) ---- */
        case GATE_MEASURE:
            /* Measurement is deferred to measure_all() after simulation. */
            break;

        /* ---- All regular gates --------------------------------- */
        default:
            dispatch_gate(instr, false, circ, NULL);
            break;
        }

        pc++;
    }

    if (is_symbolic && info.n_loops > 0) {
        symexp_htab_clear();
    }

    return true;
}

/* ================================================================== */
/*  sim_file  –  convenience wrapper (parse + simulate)                */
/* ================================================================== */

bool sim_file(FILE *in, int is_symbolic, MTBDD *circ)
{
    circuit_ir_t *ir = parse_qasm(in);
    if (!ir) {
        return false;
    }

    bool ok = simulate_ir(ir, is_symbolic, circ);
    circuit_ir_destroy(ir);
    return ok;
}

void measure_all(unsigned long samples, FILE *output, MTBDD circ, int n, int *bits_to_measure)
{
    prob_t random;
    prob_t p_qt_is_one;
    prob_t norm_coef;
    char curr_state[n+1];
    curr_state[n] = '\0';
    int curr_ct;

    htab_t *state_table = htab_init(n*n*n-1);

    for (unsigned long i=0; i < samples; i++) {
        norm_coef = 1;
        for (int j=0; j < n; j++) {
            curr_state[j] = NOT_MEASURED_CHAR;
        }

        for (int j=0; j < n; j++) {
            curr_ct = bits_to_measure[j];
            if (curr_ct ==  Q_NOT_MEASURED) {
                continue;
            }

            p_qt_is_one = measure(&circ, j, curr_state, n) * norm_coef * norm_coef;
            assert(p_qt_is_one <= (1 + EPSILON) && p_qt_is_one >= (0 - EPSILON));
            p_qt_is_one = (p_qt_is_one >= 1)? 1 : ((p_qt_is_one <= 0)? 0 : p_qt_is_one); // round
            random = (prob_t)rand() / RAND_MAX;
            if (random <= p_qt_is_one) {
                curr_state[curr_ct] = '1';
                assert(p_qt_is_one != 0);
                norm_coef *= sqrt(1/p_qt_is_one);
            }
            else {
                curr_state[curr_ct] = '0';
                assert(p_qt_is_one != 1);
                norm_coef *= sqrt(1/(1-p_qt_is_one));
            }
        }
        htab_m_lookup_add(state_table, curr_state);
    }
    htab_m_print_all(state_table, output);
    htab_m_free(state_table);
}

/* end of "sim.c" */