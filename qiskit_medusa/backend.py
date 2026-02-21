from qiskit.providers import BackendV2, JobV1
from .simulator_core import MedusaWrapper
from qiskit.transpiler.target import Target
from qiskit.circuit.library import (
    XGate, YGate, ZGate, HGate, SGate, TGate,
    CXGate, CZGate, CCXGate, MCXGate, SXGate, Measure
)
from qiskit.providers.options import Options
import uuid
from qiskit.result import Result
from qiskit.providers.jobstatus import JobStatus
from qiskit.circuit import ForLoopOp, Gate, QuantumCircuit
import numpy as np

class SYGate(Gate):
    """
    Single qubit SY gate.
    """

    def __init__(self, label=None):
        super().__init__("sy", 1, [], label=label)

    def _define(self):
        qc = QuantumCircuit(1)
        qc.ry(np.pi / 2, 0)
        self.definition = qc

class MedusaJob(JobV1):
    def __init__(self, backend, job_id, circuit, shots, wrapper, symbolic=0):
        super().__init__(backend, job_id)
        self.circuit = circuit
        self.shots = shots
        self._wrapper = wrapper
        self.symbolic = symbolic
        self._result = None
        self._handle = None

    def _add_instructions(self, circuit, qubit_map=None, clbit_map=None):
        """
        Recursively walk *circuit* and translate every instruction into
        the corresponding libmedusa C call on self._handle.

        qubit_map / clbit_map: optional dicts that remap local qubit/clbit
        indices to top-level indices (needed for control-flow bodies).
        """
        for instruction in circuit.data:
            op = instruction.operation
            name = op.name

            # resolve local -> top-level qubit / clbit indices
            local_qubits = [circuit.find_bit(q).index for q in instruction.qubits]
            local_clbits = [circuit.find_bit(c).index for c in instruction.clbits]

            qubits = [qubit_map[q] for q in local_qubits] if qubit_map else local_qubits
            clbits = [clbit_map[c] for c in local_clbits] if clbit_map else local_clbits

            # -- single-qubit gates --
            if name == 'x':
                self._wrapper.add_x(self._handle, qubits[0])
            elif name == 'y':
                self._wrapper.add_y(self._handle, qubits[0])
            elif name == 'z':
                self._wrapper.add_z(self._handle, qubits[0])
            elif name == 'h':
                self._wrapper.add_h(self._handle, qubits[0])
            elif name == 's':
                self._wrapper.add_s(self._handle, qubits[0])
            elif name == 't':
                self._wrapper.add_t(self._handle, qubits[0])
            elif name == 'sx':
                self._wrapper.add_rx_pihalf(self._handle, qubits[0])
            elif name == 'sy':
                self._wrapper.add_ry_pihalf(self._handle, qubits[0])

            # -- two-qubit gates --
            elif name == 'cx':
                self._wrapper.add_cx(self._handle, qubits[0], qubits[1])
            elif name == 'cz':
                self._wrapper.add_cz(self._handle, qubits[0], qubits[1])

            # -- three-qubit gate --
            elif name == 'ccx':
                self._wrapper.add_ccx(self._handle, qubits[0], qubits[1], qubits[2])

            # -- multi-controlled X --
            elif name == 'mcx':
                self._wrapper.add_mcx(self._handle, qubits)

            # -- measurement --
            elif name == 'measure':
                self._wrapper.add_measure(self._handle, qubits[0], clbits[0])

            # -- control flow: for loop --
            elif name == 'for_loop':
                indexset, _loop_param, body = op.params
                # map body-local qubit/clbit indices -> top-level indices
                body_qubit_map = {i: qubits[i] for i in range(len(qubits))}
                body_clbit_map = {i: clbits[i] for i in range(len(clbits))}
                for _ in indexset:
                    self._add_instructions(body, body_qubit_map, body_clbit_map)

            # -- skip no-ops --
            elif name == 'barrier':
                pass

            else:
                raise RuntimeError(f"Unsupported gate: {name}")

    def submit(self):
        """
        Submit the job to the MEDUSA simulator.
        Uses the C API directly via the wrapper's add_* methods.
        """
        self._handle = self._wrapper.circuit_create()
        try:
            # set up qubit and classical bit registers
            self._wrapper.set_qubits(self._handle, self.circuit.num_qubits)
            self._wrapper.set_bits(self._handle, self.circuit.num_clbits)

            # walk through the circuit and add gates
            self._add_instructions(self.circuit)

            # run the simulation
            mtbdd = self._wrapper.simulate_circuit(self._handle, symbolic=self.symbolic)

            # retrieve results
            counts = self._wrapper.get_counts(
                shots=self.shots,
                num_qubits=self.circuit.num_qubits,
                mtbdd=mtbdd,
            )

            # only keep non-zero counts
            counts = {k: v for k, v in counts.items() if v > 0}

            # format result
            data = {
                'counts': counts,
                'shots': self.shots,
            }

            self._result = Result.from_dict({
                'results': [
                    {
                        'shots': self.shots,
                        'success': True,
                        'data': data,
                        'header': {
                            'name': self.circuit.name,
                            'memory_slots': self.circuit.num_clbits,
                            'creg_sizes': [
                                [creg.name, creg.size]
                                for creg in self.circuit.cregs
                            ],
                        },
                    }
                ],
                'backend_name': self.backend().name,
                'backend_version': self.backend().backend_version,
                'job_id': self.job_id(),
                'qobj_id': None,
                'success': True,
            })

        except Exception as e:
            self._result = Result.from_dict({
                'results': [],
                'backend_name': self.backend().name,
                'backend_version': self.backend().backend_version,
                'job_id': self.job_id(),
                'qobj_id': None,
                'success': False,
                'status': str(e),
            })
            raise

        finally:
            # free the C-side circuit handle
            if self._handle is not None:
                self._wrapper.circuit_destroy(self._handle)
                self._handle = None

    def result(self):
        """
        Return the result of the job.
        """
        if self._result is None:
            raise RuntimeError("Job has not been submitted or is still running.")
        return self._result

    def status(self):
        # since the job runs synchronously in submit(), we can assume it's done if we have a result
        return JobStatus.DONE

class MedusaBackend(BackendV2):
    """Medusa Backend for Qiskit."""

    def __init__(self, provider = None):
        super().__init__(
            name="medusa_backend",
            description="Qiskit Backend for MEDUSA C Simulator",
            online_date=None,
            backend_version="1.0.0",
        )
        self._provider = provider
        self._wrapper = MedusaWrapper()

        # TODO: Define target with supported gates and properties
        max_n_qubits = 200  # TODO: find a reasonable default or make configurable
        self._target = Target(num_qubits=max_n_qubits)

        # {None: None} implies the gate is "ideal" (no error) and available on all qubits
        ideal_props = {None: None}

        # add supported gates to target

        # Single Qubit Gates
        self._target.add_instruction(XGate(), properties=ideal_props)
        self._target.add_instruction(YGate(), properties=ideal_props)
        self._target.add_instruction(ZGate(), properties=ideal_props)
        self._target.add_instruction(HGate(), properties=ideal_props)
        self._target.add_instruction(SGate(), properties=ideal_props)
        self._target.add_instruction(TGate(), properties=ideal_props)
        self._target.add_instruction(SXGate(), properties=ideal_props)
        self._target.add_instruction(SYGate(), properties=ideal_props)

        # Multi Qubit Gates
        self._target.add_instruction(CXGate(), properties=ideal_props)
        self._target.add_instruction(CZGate(), properties=ideal_props)
        self._target.add_instruction(CCXGate(), properties=ideal_props)
        self._target.add_instruction(MCXGate(num_ctrl_qubits=3), properties=ideal_props)

        # Control Flow
        self._target.add_instruction(ForLoopOp, name="for_loop")

        # Measurement
        self._target.add_instruction(Measure(), properties=ideal_props)

    @property
    def target(self):
        return self._target

    @property
    def max_circuits(self):
        return 1

    @classmethod
    def _default_options(cls):
        return Options(shots=1024, symbolic=0)

    @property
    def provider(self):
        return self._provider

    def run(self, run_input, **options):
        """
        Run a quantum circuit on the MEDUSA simulator backend.
        """
        # ensure run_input is a single QuantumCircuit
        if isinstance(run_input, list):
            if len(run_input) != 1:
                raise ValueError("This backend only supports a single circuit at a time.")
            circuit = run_input[0]
        else:
            circuit = run_input

        # get options
        shots = options.get("shots", self.options.shots)
        symbolic = options.get("symbolic", self.options.symbolic)

        # create and submit the job
        job_id = str(uuid.uuid4())
        job = MedusaJob(self, job_id, circuit, shots, self._wrapper, symbolic=symbolic)
        job.submit()

        return job
