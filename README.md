# qiskit-medusa
**qiskit-medusa** is a high-performance MTBDD-based quantum circuit simulator for Qiskit. It provides a Python interface to [MEDUSA](https://github.com/s-jobra/MEDUSA/) (**M**ulti-Terminal Binary D**E**cision **D**iagram-based Q**U**antum **S**imul**A**tor), a quantum simulator written in C that leverages the [Sylvan](https://trolando.github.io/sylvan/) library for efficient MTBDD operations.

This package bridges MEDUSA with Qiskit, allowing you to simulate quantum circuits using Qiskit's high-level API while benefiting from MEDUSA's efficient symbolic simulation capabilities.

## Features

- **MTBDD-based simulation**: Leverages Multi-Terminal Binary Decision Diagrams for efficient quantum state representation
- **Symbolic loop simulation**: Support for parametric quantum circuits and symbolic computation
- **Qiskit integration**: Seamless integration with Qiskit for quantum circuit design and manipulation
- **High performance**: Written in C with custom MTBDD operations via Sylvan
- **OpenQASM support**: Process circuits in OpenQASM format

## Installation

### Requirements

- Python >= 3.8
- C/C++ compiler (gcc, clang, or MSVC)
- CMake >= 3.15
- GMP library (libgmp-dev on Ubuntu/Debian)
- scikit-build-core >= 0.10

### Install via pip

Simply install the package from PyPI:

```bash
pip install qiskit-medusa
```

The installation automatically builds the C extension and compiles the MEDUSA simulator with its dependencies (Sylvan and Lace).

### Building from source

If you want to build from source:

```bash
git clone https://github.com/yourusername/qiskit-medusa
cd qiskit-medusa
pip install -e .
```

For development installation with additional tools:

```bash
pip install -e ".[dev]"
```

## Quick Start

```python
from qiskit import QuantumCircuit, transpile
from qiskit_medusa.backend import MedusaBackend

# Initialize the Medusa backend
backend = MedusaBackend()

# Enable symbolic simulation (optional)
medusa_backend.set_options(symbolic=True)

# Create a simple quantum circuit
qc = QuantumCircuit(2)
qc.h(0)
qc.cx(0, 1)
qc.measure_all()

# Transpile the circuit for the Medusa backend
transpiled_qc = transpile(qc, backend=backend)

# Run the circuit on the Medusa backend
job = backend.run(transpiled_qc, shots=5000)
result = job.result()
counts = result.get_counts()

print("Counts:", counts)
```

## Runtime Dependencies

- **Qiskit** >= 1.0: Quantum computing framework
- **NumPy** >= 1.20: Numerical computing
- **GMP**: GNU Multiple Precision Arithmetic Library (system library)
- **Sylvan**: Multi-Terminal Binary Decision Diagram library (automatically fetched during build)
- **Lace**: Work-stealing library used by Sylvan (automatically fetched during build)

## Build Dependencies

- **scikit-build-core** >= 0.10: Build backend for Python extension
- **CMake** >= 3.15: Build system
- **GMP development headers** (libgmp-dev on Ubuntu/Debian)

## Development Dependencies

- **pytest**: Testing framework
- **black**: Code formatter
- **pylint**: Linter

## Contributing

Contributions are welcome! Please feel free to open issues or submit pull requests.
