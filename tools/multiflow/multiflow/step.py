"""Per-step contract shared by the runner, the prompt and the step-mode MCP server.

The runner writes `step.json` into the step directory; the spawned agent session reports
back by calling the `multiflow_submit` MCP tool (or, as a fallback, writing `result.json`).
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Optional

from pydantic import BaseModel

from multiflow.models import now_iso
from multiflow.protection import ABSENT, matches

STEP_FILE = "step.json"
RESULT_FILE = "result.json"


class StepInput(BaseModel):
    path: str
    optional: bool = False
    exists: bool = False


class StepContext(BaseModel):
    run_id: str
    step_index: int
    attempt: int
    node: str
    role: str
    task: str
    workspace: str
    step_dir: str
    artifacts_dir: str
    allowed_verdicts: list[str]
    outputs: dict[str, str]
    inputs: dict[str, StepInput]
    locked: dict[str, str]  # name -> sha256 at step start, or "" if it must stay absent
    locked_paths: dict[str, str]
    read_only_workspace: bool = False

    @property
    def result_path(self) -> Path:
        return Path(self.step_dir) / RESULT_FILE


def step_dirname(index: int, node: str, attempt: int) -> str:
    suffix = f"-retry{attempt - 1}" if attempt > 1 else ""
    return f"{index:02d}-{node}{suffix}"


def load_context(step_dir: Path) -> StepContext:
    return StepContext.model_validate_json((step_dir / STEP_FILE).read_text())


def validate_submission(ctx: StepContext, verdict: str) -> list[str]:
    errors = []
    if verdict not in ctx.allowed_verdicts:
        errors.append(f"verdict '{verdict}' is not allowed; use one of {ctx.allowed_verdicts}")
    for name, path in ctx.outputs.items():
        p = Path(path)
        if p.is_symlink():
            errors.append(f"output '{name}' must be a regular file, not a symlink: {path}")
        elif not p.is_file() or p.stat().st_size == 0:
            errors.append(f"required output '{name}' is missing or empty: write it to {path}")
    for name, digest in ctx.locked.items():
        p = Path(ctx.locked_paths[name])
        if not matches(p, digest):
            what = "was created" if digest == ABSENT else "was modified or deleted"
            errors.append(f"locked artifact '{name}' ({p}) {what}; it is read-only for this role")
    return errors


def write_result(step_dir: Path, verdict: str, summary: str) -> None:
    (step_dir / RESULT_FILE).write_text(
        json.dumps({"verdict": verdict, "summary": summary, "submitted_at": now_iso()}, indent=2)
    )


def read_result(step_dir: Path) -> Optional[dict]:
    p = step_dir / RESULT_FILE
    if not p.is_file():
        return None
    try:
        data = json.loads(p.read_text())
    except json.JSONDecodeError:
        return None
    if not isinstance(data, dict) or not isinstance(data.get("verdict"), str):
        return None
    data.setdefault("summary", "")
    return data
