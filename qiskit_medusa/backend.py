from qiskit.providers import BackendV2, JobV1
from .simulator_core import MedusaWrapper
from qiskit.transpiler.target import Target
from qiskit.circuit.library import (
    XGate, YGate, ZGate, HGate, SGate, TGate,
    CXGate, CZGate, CCXGate, MCXGate, SXGate, Measure
)
from qiskit.providers.options import Options
import uuid
import tempfile
from qiskit.result import Result
import os
from qiskit.providers.jobstatus import JobStatus
from qiskit.qasm3 import dumps
import re
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

    def submit(self):
        """
        Submit the job to the MEDUSA simulator.
        """
        # create a temporary file to hold the circuit, use delete=False so wrapper can access it, but need to clean up later
        temp_qasm = tempfile.NamedTemporaryFile(mode='w+', delete=False, suffix='.qasm')

        try:
            # convert the circuit to OpenQASM 2.0
            qasm_str = dumps(self.circuit)

            # MEDUSA doesnt support 'barrier' instructions, so we skip them during dump
            qasm_str = re.sub(r'barrier\s+[^;]*;\n?', '', qasm_str)

            # also MEDUSA expects rx/y(pi/2) instead of sx/y, so we replace those
            qasm_str = re.sub(r"\bsx\b", "rx(pi/2)", qasm_str)
            qasm_str = re.sub(r"\bsy\b", "ry(pi/2)", qasm_str)

            # write to temporary file
            temp_qasm.write(qasm_str)
            temp_qasm.flush()
            temp_qasm.close()

            print("Simulating circuit:\n", qasm_str)
            # run the simulation
            mtbdd = self._wrapper.simulate_qasm_file(temp_qasm.name, self.symbolic)

            # retrieve results
            counts = self._wrapper.get_counts(shots=self.shots, num_qubits=self.circuit.num_qubits, mtbdd=mtbdd)

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
                            'creg_sizes': [[creg.name, creg.size] for creg in self.circuit.cregs]
                        }
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
            raise e

        finally:
            # clean up temporary file
            if os.path.exists(temp_qasm.name):
                os.remove(temp_qasm.name)

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
