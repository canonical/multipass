"""Backend interface: turn a step invocation into a headless harness session."""

from __future__ import annotations

import os
import shlex
import shutil
import subprocess
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

from multiflow.models import ToolPreset

TIMEOUT_EXIT = 124
STEP_SERVER_NAME = "multiflow-step"
# Set in every step session; the chat-mode server refuses to start nested workflows when present.
STEP_ENV_VAR = "MULTIFLOW_STEP_DIR"


@dataclass
class StepInvocation:
    prompt: str
    model: Optional[str]
    tools: ToolPreset
    workspace: Path
    step_dir: Path
    mcp_command: list[str]
    locked_paths: list[Path] = field(default_factory=list)
    deny_extra: list[str] = field(default_factory=list)
    timeout: Optional[int] = None


class Backend:
    name = "base"
    executable = ""

    def available(self) -> bool:
        return shutil.which(self.executable) is not None

    def command(self, inv: StepInvocation) -> tuple[list[str], dict[str, str]]:
        raise NotImplementedError

    def invoke(self, inv: StepInvocation) -> int:
        cmd, extra_env = self.command(inv)
        env = {**os.environ, **extra_env, STEP_ENV_VAR: str(inv.step_dir)}
        log_path = inv.step_dir / "log.txt"
        (inv.step_dir / "command.txt").write_text(
            " ".join(shlex.quote(c) if c != inv.prompt else "<prompt.md>" for c in cmd) + "\n"
        )
        with open(log_path, "wb") as log:
            try:
                proc = subprocess.run(
                    cmd, cwd=inv.workspace, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
                    env=env, timeout=inv.timeout,
                )
                return proc.returncode
            except subprocess.TimeoutExpired:
                log.write(f"\n[multiflow] step timed out after {inv.timeout}s\n".encode())
                return TIMEOUT_EXIT
            except OSError as e:
                log.write(f"\n[multiflow] failed to start {cmd[0]}: {e}\n".encode())
                return 127
