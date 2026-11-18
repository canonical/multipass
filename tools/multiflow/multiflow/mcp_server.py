"""MCP servers.

* step mode (`multiflow mcp step --step-dir D`): injected into every headless step session.
  Lets the agent write its declared outputs and submit a validated verdict.
* chat mode (`multiflow mcp`): registered in Copilot/Claude/VS Code/OpenCode so a chat can
  list recipes, launch runs in the background and inspect their status and artifacts.
"""

from __future__ import annotations

import inspect
import os
import subprocess
import sys
import time
from pathlib import Path
from typing import Annotated, Any, Optional

from pydantic import Field

try:  # mcp >= 2
    from mcp.server.mcpserver import Context
    from mcp.server.mcpserver import MCPServer as _Server
except ImportError:  # mcp 1.x
    from mcp.server.fastmcp import Context  # type: ignore[no-redef]
    from mcp.server.fastmcp import FastMCP as _Server  # type: ignore[no-redef]

from mcp.types import Completion

from multiflow import harness, loader, runner
from multiflow.backends import BACKENDS
from multiflow.backends.base import STEP_ENV_VAR
from multiflow.models import RunState
from multiflow.step import load_context, validate_submission, write_result

MAX_ARTIFACT_CHARS = 100_000
CHAT_BACKENDS = [b for b in BACKENDS if b != "fake"]


def build_step_server(step_dir: Path) -> Any:
    step_dir = step_dir.resolve()
    server = _Server("multiflow-step")

    @server.tool()
    def multiflow_step_info() -> dict:
        """Describe the current workflow step: task, role, input/output artifact paths and allowed verdicts."""
        ctx = load_context(step_dir)
        return {
            "task": ctx.task, "node": ctx.node, "role": ctx.role, "attempt": ctx.attempt,
            "allowed_verdicts": ctx.allowed_verdicts,
            "inputs": {k: v.model_dump() for k, v in ctx.inputs.items()},
            "outputs": ctx.outputs, "read_only_artifacts": ctx.locked_paths,
            "read_only_workspace": ctx.read_only_workspace,
        }

    @server.tool()
    def multiflow_write_output(name: str, content: str) -> str:
        """Write the full content of one of this step's declared output artifacts (by artifact name).

        Use this for outputs when your file-editing tools are unavailable (read-only roles)."""
        ctx = load_context(step_dir)
        if name not in ctx.outputs:
            return f"ERROR: '{name}' is not an output of this step. Outputs: {sorted(ctx.outputs)}"
        path = Path(ctx.outputs[name])
        try:
            write_artifact_safely(Path(ctx.artifacts_dir), path, content)
        except (OSError, ValueError) as e:
            return f"ERROR: {e}"
        return f"wrote {len(content)} chars to {path}"

    @server.tool()
    def multiflow_submit(verdict: str, summary: str) -> str:
        """Finish this workflow step. `verdict` must be one of the allowed verdicts; `summary` is a short
        report that later roles will see. Returns errors to fix if the step is not complete yet."""
        ctx = load_context(step_dir)
        errors = validate_submission(ctx, verdict)
        if errors:
            return "NOT ACCEPTED - fix and call multiflow_submit again:\n- " + "\n- ".join(errors)
        write_result(step_dir, verdict, summary)
        return f"ACCEPTED: verdict '{verdict}'. Your step is complete; stop now."

    return server


def write_artifact_safely(artifacts_dir: Path, path: Path, content: str) -> None:
    """Write inside artifacts_dir only, never following symlinks (a shell-capable agent could plant one)."""
    root = artifacts_dir.resolve()
    rel = path.relative_to(artifacts_dir)
    cur = root
    for part in rel.parts[:-1]:
        cur = cur / part
        if cur.is_symlink():
            raise ValueError(f"refusing to write through symlinked directory {cur}")
        cur.mkdir(exist_ok=True)
    target = root / rel
    if target.is_symlink():
        target.unlink()
    fd = os.open(target, os.O_WRONLY | os.O_CREAT | os.O_TRUNC | getattr(os, "O_NOFOLLOW", 0), 0o644)
    with os.fdopen(fd, "w") as f:
        f.write(content)


def build_chat_server(workspace: Path) -> Any:
    workspace = workspace.resolve()
    server = _Server("multiflow")

    @server.tool()
    def multiflow_list_recipes() -> dict:
        """List available workflow recipes (project .multiflow/recipes override the built-in library)."""
        out = {}
        for name in loader.list_recipes(workspace):
            try:
                r = loader.resolve(workspace, name)
                out[name] = {
                    "description": r.recipe.description,
                    "nodes": {n: spec.role for n, spec in r.recipe.nodes.items()},
                    "entry": r.recipe.entry,
                }
            except loader.LoadError as e:
                out[name] = {"error": str(e)}
        return out

    def multiflow_start(recipe: str, task: str, backend: str = "",
                        models: Optional[dict[str, str]] = None, ctx: Optional[Context] = None) -> dict:
        """Start a workflow run in the background. Each step runs as a separate headless agent session.

        Args:
            recipe: recipe name (see multiflow_list_recipes)
            task: full task description for the workflow
            backend: copilot | claude | opencode | fake; omit to use the harness this chat runs in
            models: optional model overrides, {"*": model} for all steps or {node: model}
        """
        if os.environ.get(STEP_ENV_VAR):
            return {"error": "multiflow_start is disabled inside a workflow step (no nested workflows)"}
        how = "argument"
        if not backend:
            backend, how = harness.detect(client_name(ctx))
        try:
            run_dir = runner.create_run(workspace, recipe, task, backend, models or {})
        except (loader.LoadError, runner.RunError, ValueError) as e:
            return {"error": str(e)}
        run_id = run_dir.name
        with open(run_dir / "runner.log", "ab") as log:
            proc = subprocess.Popen(
                [sys.executable, "-m", "multiflow", "--workspace", str(workspace), "resume", run_id],
                cwd=workspace, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
                start_new_session=True, env=os.environ.copy(),
            )
        # Wait until the runner holds the run lock so an immediate status call doesn't report "stopped".
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline and proc.poll() is None and not runner.is_active(
                runner.load_state(run_dir), run_dir):
            time.sleep(0.1)
        if proc.poll() not in (None, 0):
            return {"error": f"runner exited with code {proc.returncode}; see {run_dir / 'runner.log'}"}
        return {"run_id": run_id, "run_dir": str(run_dir), "backend": backend, "backend_source": how,
                "hint": "Poll multiflow_status(run_id) occasionally; steps take minutes each."}

    # Advertise the valid values as JSON-schema enums so clients and models can offer/choose them.
    # Validation stays permissive: recipes added after the server started still work (and get a hint if misspelled).
    recipe_names = sorted(loader.list_recipes(workspace))
    multiflow_start.__annotations__.update(
        recipe=Annotated[str, Field(json_schema_extra={"enum": recipe_names})] if recipe_names else str,
    )
    server.tool()(multiflow_start)
    _add_prompts(server, workspace, recipe_names)

    @server.tool()
    def multiflow_status(run_id: str = "") -> dict:
        """Status, current step and history of a run (latest run if run_id is empty)."""
        try:
            run_dir = runner.find_run(workspace, run_id or None)
        except runner.RunError as e:
            return {"error": str(e)}
        return status_dict(run_dir)

    @server.tool()
    def multiflow_list_runs(limit: int = 10) -> list[dict]:
        """List recent runs, newest first."""
        root = runner.runs_root(workspace)
        dirs = sorted((p for p in root.glob("*") if (p / "state.json").is_file()),
                      key=lambda p: p.stat().st_mtime, reverse=True)[:limit] if root.is_dir() else []
        out = []
        for d in dirs:
            s = runner.load_state(d)
            out.append({"run_id": s.run_id, "recipe": s.recipe, "status": _status_label(s, d),
                        "task": s.task[:120], "updated_at": s.updated_at})
        return out

    @server.tool()
    def multiflow_read_artifact(run_id: str, name: str) -> str:
        """Read an artifact (e.g. plan, review) produced by a run."""
        try:
            run_dir = runner.find_run(workspace, run_id or None)
        except runner.RunError as e:
            return f"ERROR: {e}"
        resolved = runner.load_resolved(run_dir)
        spec = resolved.recipe.artifacts.get(name)
        if spec is None:
            return f"ERROR: unknown artifact '{name}'. Known: {sorted(resolved.recipe.artifacts)}"
        path = run_dir / "artifacts" / spec.path
        if not path.is_file():
            return f"(artifact '{name}' not produced yet)"
        return path.read_text(errors="replace")[:MAX_ARTIFACT_CHARS]

    return server


def client_name(ctx: Optional[Context]) -> Optional[str]:
    """Name the MCP client sent in `initialize` (mcp 2: client_info, mcp 1: clientInfo), if available."""
    try:
        params = ctx.session.client_params  # type: ignore[union-attr]
        info = getattr(params, "client_info", None) or getattr(params, "clientInfo", None)
        return getattr(info, "name", None)
    except Exception:
        return None


def launcher_prompt(recipe: str, task: str, backend: str = "") -> str:
    """Instructions for a chat to launch and monitor a run (used by the MCP prompts)."""
    pick = (f"Use the recipe `{recipe}`." if recipe else
            "Call `multiflow_list_recipes` and pick the best-fitting recipe for the task; say which and why.")
    return (
        f"Run a multiflow workflow for this task:\n\n{task}\n\n"
        "Each step runs as a separate headless agent session with its own role, model and file permissions. "
        "You are only the launcher/monitor: do NOT implement the task yourself.\n\n"
        f"1. {pick}\n"
        f"2. Call `multiflow_start` with that recipe and the full task above"
        + (f" and `backend: \"{backend}\"`. " if backend else
           ". Omit `backend` unless the user asked for one: the server then uses the harness this chat runs in. ")
        + "Pass `models` only if the user asked for specific models.\n"
        "3. Report the run id; progress can also be followed with `multiflow status <run_id>` in a terminal.\n"
        "4. Poll `multiflow_status` about every 60 seconds (e.g. `sleep 60` in a terminal) until the status is no "
        "longer `running`. Then summarize each step's verdict and summary, read the final artifacts with "
        "`multiflow_read_artifact` and show the resulting changes (`git diff --stat`). If the run halted, explain "
        "`status_reason` and suggest `multiflow resume --extra-attempt <run_id>`."
    )


def _add_prompts(server: Any, workspace: Path, recipe_names: list[str]) -> None:
    """MCP prompts appear as slash commands in clients that support them (VS Code: `/mcp.multiflow.<name>`,
    Claude Code: `/mcp__multiflow__<name>`); `completion/complete` completes their arguments."""

    @server.prompt(name="run", description="Launch a multiflow workflow run and monitor it until it finishes.")
    def run_prompt(task: str, recipe: str = "", backend: str = "") -> str:
        return launcher_prompt(recipe, task, backend)

    for name in recipe_names:
        if name == "run":
            continue
        try:
            desc = loader.resolve(workspace, name).recipe.description
        except loader.LoadError:
            continue

        def recipe_prompt(task: str, backend: str = "", _recipe: str = name) -> str:
            return launcher_prompt(_recipe, task, backend)

        # Hide the bound recipe parameter from the prompt's arguments.
        recipe_prompt.__signature__ = inspect.signature(recipe_prompt).replace(  # type: ignore[attr-defined]
            parameters=list(inspect.signature(recipe_prompt).parameters.values())[:2])
        server.prompt(name=name, description=f"multiflow `{name}`: {desc}")(recipe_prompt)

    @server.completion()
    async def complete(ref: Any, argument: Any, context: Any) -> Optional[Completion]:
        if getattr(ref, "type", "") != "ref/prompt":
            return None
        if argument.name == "recipe":
            options = sorted(loader.list_recipes(workspace))
        elif argument.name == "backend":
            options = CHAT_BACKENDS
        else:
            return None
        return Completion(values=[o for o in options if o.startswith(argument.value or "")][:100])


def _status_label(state: RunState, run_dir: Path) -> str:
    if state.status == "running" and not runner.is_active(state, run_dir):
        return "stopped (resume with `multiflow resume`)"
    return state.status


def status_dict(run_dir: Path) -> dict:
    s = runner.load_state(run_dir)
    resolved = runner.load_resolved(run_dir)
    artifacts = {n: str(run_dir / "artifacts" / a.path) for n, a in resolved.recipe.artifacts.items()
                 if (run_dir / "artifacts" / a.path).is_file()}
    return {
        "run_id": s.run_id, "recipe": s.recipe, "backend": s.backend, "task": s.task,
        "status": _status_label(s, run_dir), "status_reason": s.status_reason, "current_node": s.current_node,
        "history": [h.model_dump(include={"index", "node", "role", "model", "attempt", "verdict", "summary",
                                          "rejected", "warnings", "ended_at"}) for h in s.history],
        "artifacts": artifacts, "run_dir": str(run_dir),
    }


def serve_step(step_dir: Path) -> None:
    build_step_server(step_dir).run()


def serve_chat(workspace: Path) -> None:
    build_chat_server(workspace).run()
