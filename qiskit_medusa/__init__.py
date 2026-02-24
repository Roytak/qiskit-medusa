"""Qiskit Medusa – A high-performance MTBDD quantum simulator for Qiskit."""

__version__ = "0.1.0"

from qiskit_medusa.backend import (
    AssertEq,
    AssertSuperposition,
    AssertEntangled,
    MedusaBackend,
)

__all__ = [
    "AssertEq",
    "AssertSuperposition",
    "AssertEntangled",
    "MedusaBackend",
]
