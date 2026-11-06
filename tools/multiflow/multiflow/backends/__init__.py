from multiflow.backends.base import Backend, StepInvocation
from multiflow.backends.claude import ClaudeBackend
from multiflow.backends.copilot import CopilotBackend
from multiflow.backends.fake import FakeBackend
from multiflow.backends.opencode import OpenCodeBackend

BACKENDS: dict[str, type[Backend]] = {
    "copilot": CopilotBackend,
    "claude": ClaudeBackend,
    "opencode": OpenCodeBackend,
    "fake": FakeBackend,
}


def get_backend(name: str) -> Backend:
    try:
        return BACKENDS[name]()
    except KeyError:
        raise ValueError(f"unknown backend '{name}' (available: {sorted(BACKENDS)})") from None


__all__ = ["BACKENDS", "Backend", "StepInvocation", "get_backend"]
