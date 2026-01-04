from qiskit.providers import ProviderV1
from qiskit.providers.providerutils import filter_backends

class MedusaProvider(ProviderV1):
    """Medusa Provider for Qiskit."""

    def __init__(self, token = None):
        super().__init__()
        self.token = token
        self._backends = []

    def backends(self, name=None, filters=None, **kwargs):
        """
        Return a list of backends matching the specified filtering.
        """
        # start with all backends this provider manages
        backends = self._backends

        # filter by name if provided
        if name:
            backends = [b for b in backends if b.name == name]

        # use Qiskit's utility to filter by other criteria (like n_qubits, etc.)
        return filter_backends(backends, filters=filters, **kwargs)
