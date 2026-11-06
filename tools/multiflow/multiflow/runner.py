"""Run loop: prepare step -> invoke backend -> verify -> advance the state machine.

Run layout (`<workspace>/.multiflow/runs/<run-id>/`):
  state.json        live RunState (resumable)
  recipe.json       recipe + roles frozen at start; edits to library files don't affect a live run
  artifacts/        shared blackboard between roles
  steps/NN-node/    step.json, prompt.md, command.txt, log.txt, result.json, locked-snapshot/
"""

from __future__ import annotations

import contextlib
import os
import secrets
import sys
import time
from pathlib import Path
from typing import Callable, Iterator, Optional

try:
    import fcntl
except ImportError:  # Windows: fall back to pid checks only
    fcntl = None  # type: ignore[assignment]

from multiflow import engine, loader, protection
from multiflow.backends import Backend, StepInvocation
from multiflow.models import ResolvedRecipe, RunState, StepRecord, now_iso
from multiflow.prompt import build_prompt
from multiflow.step import STEP_FILE, StepContext, StepInput, read_result, step_dirname, validate_submission

RUNS_DIRNAME = "runs"
Log = Callable[[str], None]


class RunError(Exception):
    pass


def runs_root(workspace: Path) -> Path:
    return workspace / loader.PROJECT_DIRNAME / RUNS_DIRNAME


def new_run_id(recipe: str) -> str:
    return f"{time.strftime('%Y%m%d-%H%M%S')}-{recipe}-{secrets.token_hex(2)}"


def _write_atomic(path: Path, text: str) -> None:
    tmp = path.with_name(f".{path.name}.{os.getpid()}.{secrets.token_hex(3)}.tmp")
    tmp.write_text(text)
    os.replace(tmp, path)


def save_state(run_dir: Path, state: RunState) -> None:
    state.touch()
    _write_atomic(run_dir / "state.json", state.model_dump_json(indent=2))


def load_state(run_dir: Path) -> RunState:
    return RunState.model_validate_json((run_dir / "state.json").read_text())


def load_resolved(run_dir: Path) -> ResolvedRecipe:
    return ResolvedRecipe.model_validate_json((run_dir / "recipe.json").read_text())


def find_run(workspace: Path, run_id: Optional[str]) -> Path:
    root = runs_root(workspace)
    if run_id:
        d = root / run_id
        if not (d / "state.json").is_file():
            raise RunError(f"run '{run_id}' not found under {root}")
        return d
    runs = sorted(p for p in root.glob("*") if (p / "state.json").is_file()) if root.is_dir() else []
    if not runs:
        raise RunError(f"no runs under {root}")
    return max(runs, key=lambda p: (p / "state.json").stat().st_mtime)


def create_run(
    workspace: Path, recipe_name: str, task: str, backend: str,
    model_overrides: Optional[dict[str, str]] = None, run_id: Optional[str] = None,
) -> Path:
    resolved = loader.resolve(workspace, recipe_name)
    overrides = model_overrides or {}
    for key in overrides:
        if key != "*" and key not in resolved.recipe.nodes:
            raise RunError(f"--model override for unknown node '{key}' (nodes: {sorted(resolved.recipe.nodes)})")
    run_id = run_id or new_run_id(recipe_name)
    run_dir = runs_root(workspace) / run_id
    if run_dir.exists():
        raise RunError(f"run directory already exists: {run_dir}")
    (run_dir / "artifacts").mkdir(parents=True)
    (run_dir / "steps").mkdir()
    _write_atomic(run_dir / "recipe.json", resolved.model_dump_json(indent=2))
    state = RunState(
        run_id=run_id, recipe=recipe_name, task=task, backend=backend,
        workspace=str(workspace.resolve()), model_overrides=overrides,
    )
    engine.start(resolved, state)
    save_state(run_dir, state)
    return run_dir


def _pid_alive(pid: Optional[int]) -> bool:
    if not pid:
        return False
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


LOCK_FILE = "runner.lock"


@contextlib.contextmanager
def run_lock(run_dir: Path) -> Iterator[None]:
    """Exclusive per-run lock held for the whole execution, so two runners never drive the same run."""
    if fcntl is None:
        yield
        return
    with open(run_dir / LOCK_FILE, "a") as f:
        try:
            fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            raise RunError(f"run {run_dir.name} is already being executed by another process") from None
        try:
            yield
        finally:
            fcntl.flock(f, fcntl.LOCK_UN)


def _lock_held(run_dir: Path) -> bool:
    if fcntl is None or not (run_dir / LOCK_FILE).exists():
        return False
    with open(run_dir / LOCK_FILE, "a") as f:
        try:
            fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            return True
        fcntl.flock(f, fcntl.LOCK_UN)
        return False


def is_active(state: RunState, run_dir: Optional[Path] = None) -> bool:
    if state.status != "running":
        return False
    if run_dir is not None and fcntl is not None:
        return _lock_held(run_dir)
    return _pid_alive(state.pid)


def mcp_step_command(step_dir: Path) -> list[str]:
    return [sys.executable, "-m", "multiflow", "mcp", "step", "--step-dir", str(step_dir)]


def execute(run_dir: Path, backend: Backend, timeout: Optional[int] = None, log: Log = lambda m: None) -> RunState:
    run_dir = run_dir.resolve()
    with run_lock(run_dir):
        return _execute_locked(run_dir, backend, timeout, log)


def _execute_locked(run_dir: Path, backend: Backend, timeout: Optional[int], log: Log) -> RunState:
    state = load_state(run_dir)
    resolved = load_resolved(run_dir)
    if state.status != "running":
        log(f"run {state.run_id} is already {state.status}: {state.status_reason}")
        return state
    if fcntl is None and state.pid and state.pid != os.getpid() and _pid_alive(state.pid):
        raise RunError(f"run {state.run_id} is already being executed by pid {state.pid}")
    if not backend.available():
        raise RunError(f"backend '{backend.name}' is not available (is `{backend.executable}` on PATH?)")
    state.pid = os.getpid()

    # Resume after a crash/interrupt: the unfinished step is re-run without counting another visit.
    last = state.history[-1] if state.history else None
    if last and last.ended_at is None:
        last.ended_at, last.rejected = now_iso(), "interrupted (runner stopped before the step finished)"
        state.attempt = last.attempt
    save_state(run_dir, state)

    try:
        while state.status == "running":
            _run_one_step(run_dir, resolved, state, backend, timeout, log)
            save_state(run_dir, state)
    finally:
        state.pid = None
        save_state(run_dir, state)
    log(f"run {state.run_id}: {state.status.upper()} ({state.status_reason})")
    return state


def _halt(state: RunState, reason: str) -> None:
    state.status, state.status_reason = "halted", reason


def _run_one_step(
    run_dir: Path, resolved: ResolvedRecipe, state: RunState, backend: Backend, timeout: Optional[int], log: Log,
) -> None:
    recipe = resolved.recipe
    node_name = state.current_node
    assert node_name is not None
    node = recipe.nodes[node_name]
    role = resolved.roles[node.role]

    last = state.history[-1] if state.history else None
    same_node_rejected = bool(last and last.node == node_name and last.rejected)
    resuming = same_node_rejected and last.rejected.startswith("interrupted")  # type: ignore[union-attr]
    if state.attempt == 1 and not resuming and not state.bonus_attempt:
        if reason := engine.enter_node(resolved, state):
            _halt(state, reason)
            return
    feedback = last.rejected if (same_node_rejected and (state.attempt > 1 or state.bonus_attempt)) else ""

    workspace = Path(state.workspace)
    artifacts_dir = run_dir / "artifacts"
    art_paths = {name: artifacts_dir / spec.path for name, spec in recipe.artifacts.items()}
    for name in node.outputs:
        art_paths[name].parent.mkdir(parents=True, exist_ok=True)

    state.step_index += 1
    step_dir = run_dir / "steps" / step_dirname(state.step_index, node_name, state.attempt)
    step_dir.mkdir(parents=True, exist_ok=True)

    model = engine.resolve_model(resolved, node_name, backend.name, state.model_overrides)
    tools = engine.resolve_tools(resolved, node_name)
    # Persist the record before any fallible setup so a crash is resumed as "interrupted", not a new visit.
    record = StepRecord(index=state.step_index, node=node_name, role=role.name, model=model,
                        attempt=state.attempt, step_dir=str(step_dir.relative_to(run_dir)))
    state.history.append(record)
    save_state(run_dir, state)

    locked = protection.lock(
        {n: p for n, p in art_paths.items() if n not in node.outputs}, step_dir / "locked-snapshot"
    )
    ctx = StepContext(
        run_id=state.run_id, step_index=state.step_index, attempt=state.attempt, node=node_name, role=role.name,
        task=state.task, workspace=str(workspace), step_dir=str(step_dir), artifacts_dir=str(artifacts_dir),
        allowed_verdicts=list(role.verdicts),
        outputs={n: str(art_paths[n]) for n in node.outputs},
        inputs={n: StepInput(path=str(art_paths[n]), optional=opt, exists=art_paths[n].is_file())
                for n, opt in node.input_names},
        locked={n: e[1] for n, e in locked.entries.items()},
        locked_paths={n: str(e[0]) for n, e in locked.entries.items()},
        read_only_workspace=(tools == "read-only"),
    )
    (step_dir / STEP_FILE).write_text(ctx.model_dump_json(indent=2))
    prompt = build_prompt(resolved, state, ctx, feedback)
    (step_dir / "prompt.md").write_text(prompt)
    log(f"[{state.step_index}] {node_name} (role={role.name}, model={model or 'default'}, tools={tools}"
        f"{', attempt ' + str(state.attempt) if state.attempt > 1 else ''}) ...")

    ws_before = protection.workspace_fingerprint(workspace) if tools == "read-only" else None
    inv = StepInvocation(
        prompt=prompt, model=model, tools=tools, workspace=workspace, step_dir=step_dir,
        mcp_command=mcp_step_command(step_dir), locked_paths=[e[0] for e in locked.entries.values()],
        deny_extra=list(role.deny_tools.get(backend.name, [])), timeout=timeout,
    )
    try:
        record.exit_code = backend.invoke(inv)
    finally:
        tampered = locked.verify_and_restore()
        locked.unlock()
    record.ended_at = now_iso()

    if ws_before is not None and (ws_after := protection.workspace_fingerprint(workspace)) is not None:
        modified, new_files = ws_before.compare(ws_after)
        if new_files:
            shown = ", ".join(new_files[:10]) + (" ..." if len(new_files) > 10 else "")
            record.warnings.append(f"read-only role created untracked files: {shown}")
            log(f"    warning: {record.warnings[-1]}")
        if modified:
            record.rejected = (f"read-only role '{role.name}' modified the source tree ({', '.join(modified[:10])}); "
                               "changes were NOT reverted, inspect the working tree")
            _halt(state, record.rejected)
            log(f"    HALT: {record.rejected}")
            return

    problems = []
    if tampered:
        problems.append(f"modified locked artifacts {tampered} (reverted)")
    result = read_result(step_dir)
    if result is None:
        problems.append(f"did not submit a result via multiflow_submit (exit code {record.exit_code}, "
                        f"see {step_dir / 'log.txt'})")
    else:
        problems += validate_submission(ctx, result["verdict"])

    bonus, state.bonus_attempt = state.bonus_attempt, False
    if problems:
        record.rejected = "; ".join(problems)
        log(f"    rejected: {record.rejected}")
        if bonus:
            _halt(state, f"step '{node_name}' rejected on its extra attempt: {record.rejected}")
        elif state.attempt <= recipe.max_step_retries:
            state.attempt += 1
        else:
            _halt(state, f"step '{node_name}' rejected {state.attempt} time(s): {record.rejected}")
        return

    assert result is not None
    record.verdict, record.summary = result["verdict"], result.get("summary", "")
    log(f"    -> {record.verdict}: {record.summary.strip()[:200]}")
    engine.advance(resolved, state, record.verdict)


def parse_model_overrides(values: list[str]) -> dict[str, str]:
    """`--model X` applies to all nodes; `--model node=X` to one node."""
    out: dict[str, str] = {}
    for v in values:
        node, sep, model = v.partition("=")
        if sep:
            out[node.strip()] = model.strip()
        else:
            out["*"] = v.strip()
    return out
