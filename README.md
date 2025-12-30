# qiskit-medusa
**qiskit-medusa** is a high-performance MTBDD-based quantum circuit simulator for Qiskit. It provides a Python interface to [MEDUSA](https://github.com/trolando/medusa) (**M**ulti-Terminal Binary D**E**cision **D**iagram-based Q**U**antum **S**imul**A**tor), a quantum simulator written in C that leverages the [Sylvan](https://trolando.github.io/sylvan/) library for efficient MTBDD operations.

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
- C compiler (gcc, clang, or MSVC)
- CMake >= 3.15
- GMP library (libgmp-dev on Ubuntu/Debian)
- ninja (build system)

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
from qiskit import QuantumCircuit, QuantumRegister, ClassicalRegister
from qiskit_medusa import MedusaSimulator

# Create a simple quantum circuit
qr = QuantumRegister(2, 'q')
cr = ClassicalRegister(2, 'c')
qc = QuantumCircuit(qr, cr)
qc.h(qr[0])
qc.cx(qr[0], qr[1])
qc.measure(qr, cr)

# Simulate using MEDUSA backend
simulator = MedusaSimulator()
job = simulator.run(qc, shots=1000)
result = job.result()
counts = result.get_counts(qc)

print(counts)
```

## Dependencies

- **Qiskit** >= 1.0: Quantum computing framework
- **NumPy** >= 1.20: Numerical computing
- **GMP**: GNU Multiple Precision Arithmetic Library
- **Sylvan**: Multi-Terminal Binary Decision Diagram library (automatically fetched during build)
- **Lace**: Work-stealing library used by Sylvan (automatically fetched during build)

## Contributing

Contributions are welcome! Please feel free to open issues or submit pull requests.
