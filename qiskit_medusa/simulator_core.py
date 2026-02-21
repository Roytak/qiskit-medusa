import platform
import pathlib
import sysconfig
import ctypes
from ctypes import c_int, c_uint32, c_char_p, c_void_p

# determine the so filename
lib_filename = "libmedusa.so" if platform.system() == "Linux" else "libmedusa.dylib"

# define where to look for it
search_paths = [
    # a) look in the site-packages/qiskit_medusa/ (editable install)
    pathlib.Path(sysconfig.get_path("purelib")) / "qiskit_medusa" / lib_filename,

    # b) look in the same directory as this file (standard install)
    pathlib.Path(__file__).parent / lib_filename,
]

# find and load the library
lib_path = None
for path in search_paths:
    if path.exists():
        lib_path = path
        break

if not lib_path:
    checked_paths = ", ".join(str(p) for p in search_paths)
    raise FileNotFoundError(f"Could not find {lib_filename}. Searched in:\n{checked_paths}")

lib = ctypes.CDLL(str(lib_path))

# -- Type Definitions and Function Bindings --

# define MTBDD as uint64_t
MTBDD = ctypes.c_uint64

# define circuit IR handle type as opaque pointer
CIRCUIT_HANDLE = c_void_p

# void medusa_init(void);
lib.medusa_init.argtypes = []
lib.medusa_init.restype = None

# void medusa_destroy(void);
lib.medusa_destroy.argtypes = []
lib.medusa_destroy.restype = None

# -- Circuit handle lifecycle --

# circuit_ir_t *medusa_circuit_create(void);
lib.medusa_circuit_create.argtypes = []
lib.medusa_circuit_create.restype = CIRCUIT_HANDLE

# void medusa_circuit_destroy(circuit_ir_t *ir);
lib.medusa_circuit_destroy.argtypes = [CIRCUIT_HANDLE]
lib.medusa_circuit_destroy.restype = None

# -- Qubit / bit register setup --

# int medusa_set_qubits(circuit_ir_t *ir, uint32_t n);
lib.medusa_set_qubits.argtypes = [CIRCUIT_HANDLE, c_uint32]
lib.medusa_set_qubits.restype = c_int

# int medusa_set_bits(circuit_ir_t *ir, uint32_t n);
lib.medusa_set_bits.argtypes = [CIRCUIT_HANDLE, c_uint32]
lib.medusa_set_bits.restype = c_int

# -- Single-qubit gates --

# int medusa_add_x(circuit_ir_t *ir, uint32_t qt);
lib.medusa_add_x.argtypes = [CIRCUIT_HANDLE, c_uint32]
lib.medusa_add_x.restype = c_int

# int medusa_add_y(circuit_ir_t *ir, uint32_t qt);
lib.medusa_add_y.argtypes = [CIRCUIT_HANDLE, c_uint32]
lib.medusa_add_y.restype = c_int

# int medusa_add_z(circuit_ir_t *ir, uint32_t qt);
lib.medusa_add_z.argtypes = [CIRCUIT_HANDLE, c_uint32]
lib.medusa_add_z.restype = c_int

# int medusa_add_h(circuit_ir_t *ir, uint32_t qt);
lib.medusa_add_h.argtypes = [CIRCUIT_HANDLE, c_uint32]
lib.medusa_add_h.restype = c_int

# int medusa_add_s(circuit_ir_t *ir, uint32_t qt);
lib.medusa_add_s.argtypes = [CIRCUIT_HANDLE, c_uint32]
lib.medusa_add_s.restype = c_int

# int medusa_add_t(circuit_ir_t *ir, uint32_t qt);
lib.medusa_add_t.argtypes = [CIRCUIT_HANDLE, c_uint32]
lib.medusa_add_t.restype = c_int

# int medusa_add_rx_pihalf(circuit_ir_t *ir, uint32_t qt);
lib.medusa_add_rx_pihalf.argtypes = [CIRCUIT_HANDLE, c_uint32]
lib.medusa_add_rx_pihalf.restype = c_int

# int medusa_add_ry_pihalf(circuit_ir_t *ir, uint32_t qt);
lib.medusa_add_ry_pihalf.argtypes = [CIRCUIT_HANDLE, c_uint32]
lib.medusa_add_ry_pihalf.restype = c_int

# -- Two-qubit gates --

# int medusa_add_cx(circuit_ir_t *ir, uint32_t qc, uint32_t qt);
lib.medusa_add_cx.argtypes = [CIRCUIT_HANDLE, c_uint32, c_uint32]
lib.medusa_add_cx.restype = c_int

# int medusa_add_cz(circuit_ir_t *ir, uint32_t qc, uint32_t qt);
lib.medusa_add_cz.argtypes = [CIRCUIT_HANDLE, c_uint32, c_uint32]
lib.medusa_add_cz.restype = c_int

# -- Three-qubit gates --

# int medusa_add_ccx(circuit_ir_t *ir, uint32_t qc1, uint32_t qc2, uint32_t qt);
lib.medusa_add_ccx.argtypes = [CIRCUIT_HANDLE, c_uint32, c_uint32, c_uint32]
lib.medusa_add_ccx.restype = c_int

# -- Multi-qubit gates --

# int medusa_add_mcx(circuit_ir_t *ir, uint32_t *qubits, uint32_t n_qubits);
lib.medusa_add_mcx.argtypes = [CIRCUIT_HANDLE, ctypes.POINTER(c_uint32), c_uint32]
lib.medusa_add_mcx.restype = c_int

# -- Measurement --

# int medusa_add_measure(circuit_ir_t *ir, uint32_t qt, uint32_t ct);
lib.medusa_add_measure.argtypes = [CIRCUIT_HANDLE, c_uint32, c_uint32]
lib.medusa_add_measure.restype = c_int

# -- Simulation --

# int medusa_simulate_file(const char *filename, int symbolic, MTBDD *mtbdd);
lib.medusa_simulate_file.argtypes = [c_char_p, c_int, ctypes.POINTER(MTBDD)]
lib.medusa_simulate_file.restype = c_int

# int medusa_simulate_circuit(circuit_ir_t *ir, int symbolic, MTBDD *mtbdd);
lib.medusa_simulate_circuit.argtypes = [CIRCUIT_HANDLE, c_int, ctypes.POINTER(MTBDD)]
lib.medusa_simulate_circuit.restype = c_int

# -- Results --

# int medusa_get_counts(MTBDD mtbdd, int num_qubits, char **indices[], double **probs);
lib.medusa_get_counts.argtypes = [MTBDD,
                                   c_int,
                                   ctypes.POINTER(ctypes.POINTER(ctypes.c_char_p)),
                                   ctypes.POINTER(ctypes.POINTER(ctypes.c_double))]
lib.medusa_get_counts.restype = c_int

# void medusa_free_counts(char **indices, double *probs);
lib.medusa_free_counts.argtypes = [ctypes.POINTER(ctypes.c_char_p),
                                    ctypes.POINTER(ctypes.c_double)]
lib.medusa_free_counts.restype = None

# -- Wrapper Class --
class MedusaWrapper:
    def __init__(self):
        # initialize library
        lib.medusa_init()

    def __del__(self):
        # destroy context
        lib.medusa_destroy()

    # -- Circuit handle lifecycle --

    def circuit_create(self):
        """Creates a new, empty circuit IR handle."""
        handle = lib.medusa_circuit_create()
        if not handle:
            raise RuntimeError("Failed to create circuit handle")
        return handle

    def circuit_destroy(self, handle):
        """Destroys a circuit IR handle and frees associated memory."""
        lib.medusa_circuit_destroy(handle)

    # -- Qubit / bit register setup --

    def set_qubits(self, handle, n: int):
        """Sets the number of qubits in the circuit."""
        if lib.medusa_set_qubits(handle, n) != 0:
            raise RuntimeError("Failed to set qubits")

    def set_bits(self, handle, n: int):
        """Sets the number of classical bits in the circuit."""
        if lib.medusa_set_bits(handle, n) != 0:
            raise RuntimeError("Failed to set bits")

    # -- Single-qubit gates --

    def add_x(self, handle, qt: int):
        """Adds a Pauli-X gate on qubit qt."""
        if lib.medusa_add_x(handle, qt) != 0:
            raise RuntimeError("Failed to add X gate")

    def add_y(self, handle, qt: int):
        """Adds a Pauli-Y gate on qubit qt."""
        if lib.medusa_add_y(handle, qt) != 0:
            raise RuntimeError("Failed to add Y gate")

    def add_z(self, handle, qt: int):
        """Adds a Pauli-Z gate on qubit qt."""
        if lib.medusa_add_z(handle, qt) != 0:
            raise RuntimeError("Failed to add Z gate")

    def add_h(self, handle, qt: int):
        """Adds a Hadamard gate on qubit qt."""
        if lib.medusa_add_h(handle, qt) != 0:
            raise RuntimeError("Failed to add H gate")

    def add_s(self, handle, qt: int):
        """Adds an S gate on qubit qt."""
        if lib.medusa_add_s(handle, qt) != 0:
            raise RuntimeError("Failed to add S gate")

    def add_t(self, handle, qt: int):
        """Adds a T gate on qubit qt."""
        if lib.medusa_add_t(handle, qt) != 0:
            raise RuntimeError("Failed to add T gate")

    def add_rx_pihalf(self, handle, qt: int):
        """Adds an RX(pi/2) gate on qubit qt."""
        if lib.medusa_add_rx_pihalf(handle, qt) != 0:
            raise RuntimeError("Failed to add RX(pi/2) gate")

    def add_ry_pihalf(self, handle, qt: int):
        """Adds an RY(pi/2) gate on qubit qt."""
        if lib.medusa_add_ry_pihalf(handle, qt) != 0:
            raise RuntimeError("Failed to add RY(pi/2) gate")

    # -- Two-qubit gates --

    def add_cx(self, handle, qc: int, qt: int):
        """Adds a controlled-X (CNOT) gate with control qc and target qt."""
        if lib.medusa_add_cx(handle, qc, qt) != 0:
            raise RuntimeError("Failed to add CX gate")

    def add_cz(self, handle, qc: int, qt: int):
        """Adds a controlled-Z gate with control qc and target qt."""
        if lib.medusa_add_cz(handle, qc, qt) != 0:
            raise RuntimeError("Failed to add CZ gate")

    # -- Three-qubit gates --

    def add_ccx(self, handle, qc1: int, qc2: int, qt: int):
        """Adds a Toffoli (CCX) gate with controls qc1, qc2 and target qt."""
        if lib.medusa_add_ccx(handle, qc1, qc2, qt) != 0:
            raise RuntimeError("Failed to add CCX gate")

    # -- Multi-qubit gates --

    def add_mcx(self, handle, qubits: list):
        """Adds a multi-controlled X gate. qubits = [target, ctrl1, ctrl2, ...]."""
        n = len(qubits)
        arr = (c_uint32 * n)(*qubits)
        if lib.medusa_add_mcx(handle, arr, n) != 0:
            raise RuntimeError("Failed to add MCX gate")

    # -- Measurement --

    def add_measure(self, handle, qt: int, ct: int):
        """Adds a measurement of qubit qt into classical bit ct."""
        if lib.medusa_add_measure(handle, qt, ct) != 0:
            raise RuntimeError("Failed to add measure")

    # -- Assertions --

    def add_assert_eq(self, handle, state_str: str, prob_threshold: float):
        """Adds an assertion that a given quantum state has a certain probability."""
        b_state_str = state_str.encode('utf-8')
        if lib.medusa_add_assert_eq(handle, b_state_str, prob_threshold) != 0:
            raise RuntimeError("Failed to add assert_eq")

    def add_assert_sup(self, handle, qubits: list):
        """Adds an assertion that a given set of qubits is in a superposition state."""
        n = len(qubits)
        arr = (c_uint32 * n)(*qubits)
        if lib.medusa_add_assert_sup(handle, arr, n) != 0:
            raise RuntimeError("Failed to add assert_sup")

    def add_assert_ent(self, handle, qubits: list):
        """Adds an assertion that a given set of qubits is in an entangled state."""
        n = len(qubits)
        arr = (c_uint32 * n)(*qubits)
        if lib.medusa_add_assert_ent(handle, arr, n) != 0:
            raise RuntimeError("Failed to add assert_ent")

    # -- Simulation --

    def simulate_circuit(self, handle, symbolic: bool = False):
        """Simulates a circuit built via the handle API. Returns the MTBDD."""
        mtbdd = MTBDD()
        res = lib.medusa_simulate_circuit(handle, 1 if symbolic else 0, ctypes.byref(mtbdd))
        if res != 0:
            raise RuntimeError("Circuit simulation failed")
        return mtbdd

    def get_counts(self, shots = 1024, num_qubits=None, mtbdd=None):
        if mtbdd is None:
            raise ValueError("MTBDD must be provided to get counts")

        indices_ptr = ctypes.POINTER(ctypes.c_char_p)()
        probs_ptr = ctypes.POINTER(ctypes.c_double)()

        res = lib.medusa_get_counts(mtbdd,
                                    num_qubits if num_qubits is not None else 0,
                                    ctypes.byref(indices_ptr),
                                    ctypes.byref(probs_ptr))
        if res != 0:
            raise RuntimeError("Failed to get counts from simulation")

        # convert to Python lists
        counts = {}
        idx = 0
        while True:
            index = indices_ptr[idx]
            if index is None:
                break

            # decode to bitstring
            bitstring = index.decode('utf-8')
            prob = probs_ptr[idx]

            # round to nearest integer count
            counts[bitstring] = int(round(prob * shots))

            idx += 1

        # free allocated memory
        lib.medusa_free_counts(indices_ptr, probs_ptr)

        return counts
