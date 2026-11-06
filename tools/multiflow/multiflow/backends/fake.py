"""In-process backend for tests and dry runs: a Python callable plays the agent."""

from __future__ import annotations

from typing import Callable

from multiflow.backends.base import Backend, StepInvocation
from multiflow.step import StepContext, load_context

Agent = Callable[[StepInvocation, StepContext], int]


def _noop_agent(inv: StepInvocation, ctx: StepContext) -> int:
    """Default dry-run agent: writes placeholder outputs and submits the first verdict."""
    from multiflow.step import write_result

    for path in ctx.outputs.values():
        with open(path, "w") as f:
            f.write(f"dry-run output for {ctx.node}\n")
    write_result(inv.step_dir, ctx.allowed_verdicts[0], f"dry run of {ctx.node}")
    return 0


class FakeBackend(Backend):
    name = "fake"

    def __init__(self, agent: Agent = _noop_agent):
        self.agent = agent
        self.invocations: list[StepInvocation] = []

    def available(self) -> bool:
        return True

    def invoke(self, inv: StepInvocation) -> int:
        self.invocations.append(inv)
        (inv.step_dir / "log.txt").write_text("fake backend\n")
        return self.agent(inv, load_context(inv.step_dir))
