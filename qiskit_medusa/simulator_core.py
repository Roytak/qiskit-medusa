import platform
import pathlib
import sysconfig
import ctypes
from ctypes import c_int, c_char_p, c_void_p

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

# void medusa_init(void);
lib.medusa_init.argtypes = []
lib.medusa_init.restype = None

# void medusa_destroy(void);
lib.medusa_destroy.argtypes = []
lib.medusa_destroy.restype = None

# int int medusa_simulate_file(const char *filename, int symbolic, MTBDD *mtbdd);
lib.medusa_simulate_file.argtypes = [c_char_p, c_int, ctypes.POINTER(MTBDD)]
lib.medusa_simulate_file.restype = c_int

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

    def simulate_qasm_file(self, filename: str, symbolic: bool = False):
        # prepare args
        b_filename = filename.encode('utf-8')
        symbolic = 1 if symbolic else 0
        mtbdd = MTBDD()

        # run simulation
        res = lib.medusa_simulate_file(b_filename, symbolic, ctypes.byref(mtbdd))
        if res != 0:
            raise RuntimeError(f"Simulation failed for file: {filename}")

        return mtbdd
