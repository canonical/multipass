import pytest


@pytest.fixture(autouse=True, scope="session")
def ensure_sudo_auth():
    """Override parent fixture - unit tests don't need sudo."""
    yield


@pytest.fixture(autouse=True, scope="session")
def ensure_multipass_binaries_are_present():
    """Override parent fixture - unit tests don't need multipass binaries."""
    yield


@pytest.fixture(autouse=True, scope="session")
def environment_setup():
    """Override parent fixture - unit tests don't need environment setup."""
    yield
