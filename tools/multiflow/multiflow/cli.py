"""Command-line interface: `multiflow run|resume|status|list|show|validate|mcp`."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from multiflow import loader, runner
from multiflow.backends import BACKENDS, get_backend
from multiflow.models import COMPLETE, TERMINALS


def _log(msg: str) -> None:
    print(msg, flush=True)


def _workspace(args: argparse.Namespace) -> Path:
    return Path(args.workspace).resolve()


def cmd_run(args: argparse.Namespace) -> int:
    ws = _workspace(args)
    task = Path(args.task_file).read_text() if args.task_file else args.task
    if not task:
        print("error: provide a TASK or --task-file", file=sys.stderr)
        return 2
    backend = get_backend(args.backend)
    if not backend.available():
        print(f"error: backend '{backend.name}' not available (`{backend.executable}` not on PATH)", file=sys.stderr)
        return 2
    run_dir = runner.create_run(ws, args.recipe, task, args.backend, runner.parse_model_overrides(args.model or []))
    _log(f"run {run_dir.name} -> {run_dir}")
    state = runner.execute(run_dir, backend, timeout=args.timeout, log=_log)
    _print_artifacts(run_dir)
    return 0 if state.status == "completed" else 1


def cmd_resume(args: argparse.Namespace) -> int:
    ws = _workspace(args)
    run_dir = runner.find_run(ws, args.run_id)
    state = runner.load_state(run_dir)
    if args.extra_attempt and state.status == "halted" and state.current_node:
        # Give a halted step exactly one more chance (e.g. after fixing the environment or raising a limit).
        state.status, state.status_reason, state.bonus_attempt = "running", "", True
        runner.save_state(run_dir, state)
    backend = get_backend(args.backend or state.backend)
    state = runner.execute(run_dir, backend, timeout=args.timeout, log=_log)
    _print_artifacts(run_dir)
    return 0 if state.status == "completed" else 1


def _print_artifacts(run_dir: Path) -> None:
    arts = sorted(p for p in (run_dir / "artifacts").rglob("*") if p.is_file())
    if arts:
        _log("artifacts:")
        for p in arts:
            _log(f"  {p}")


def cmd_status(args: argparse.Namespace) -> int:
    from multiflow.mcp_server import status_dict

    run_dir = runner.find_run(_workspace(args), args.run_id)
    st = status_dict(run_dir)
    if args.json:
        print(json.dumps(st, indent=2))
        return 0
    print(f"run:     {st['run_id']}  ({st['recipe']} on {st['backend']})")
    print(f"status:  {st['status']}{'  - ' + st['status_reason'] if st['status_reason'] else ''}")
    if st["current_node"]:
        print(f"current: {st['current_node']}")
    print(f"task:    {st['task'].strip().splitlines()[0][:100] if st['task'].strip() else ''}")
    for h in st["history"]:
        outcome = h["verdict"] or (f"REJECTED: {h['rejected']}" if h["rejected"] else "(running)")
        print(f"  [{h['index']}] {h['node']:<12} {h['role']:<12} {h['model'] or 'default':<20} {outcome}")
        if h["summary"]:
            print(f"        {h['summary'].strip().splitlines()[0][:110]}")
        for w in h.get("warnings") or []:
            print(f"        warning: {w[:110]}")
    for name, path in st["artifacts"].items():
        print(f"  artifact {name}: {path}")
    return 0


def cmd_list(args: argparse.Namespace) -> int:
    ws = _workspace(args)
    if args.what == "runs":
        root = runner.runs_root(ws)
        for d in sorted(root.glob("*")) if root.is_dir() else []:
            if (d / "state.json").is_file():
                s = runner.load_state(d)
                print(f"{s.run_id:<45} {s.status:<10} {s.task.strip()[:60]!r}")
        return 0
    items = loader.list_recipes(ws) if args.what == "recipes" else loader.list_roles(ws)
    for name, path in items.items():
        try:
            desc = (loader.load_recipe(ws, name) if args.what == "recipes" else loader.load_role(ws, name)).description
        except loader.LoadError as e:
            desc = f"INVALID: {e}"
        print(f"{name:<16} {desc.strip().splitlines()[0] if desc.strip() else ''}  [{path}]")
    return 0


def cmd_show(args: argparse.Namespace) -> int:
    ws = _workspace(args)
    r = loader.resolve(ws, args.recipe)
    rec = r.recipe
    print(f"{rec.name}: {rec.description.strip()}")
    print(f"max_iterations={rec.max_iterations} max_step_retries={rec.max_step_retries}  entry={rec.entry}")
    for name, node in rec.nodes.items():
        role = r.roles[node.role]
        model = node.model or role.model or "default"
        print(f"\n  [{name}] role={role.name} tools={node.tools or role.tools} model={model}")
        if node.inputs:
            print(f"     in:  {', '.join(node.inputs)}")
        if node.outputs:
            print(f"     out: {', '.join(node.outputs)}")
        locked = [a for a in rec.artifacts if a not in node.outputs]
        if locked:
            print(f"     read-only: {', '.join(locked)}")
        for verdict, dst in rec.transitions[name].items():
            print(f"     --{verdict}--> {dst if dst not in TERMINALS else ('DONE' if dst == COMPLETE else 'FAIL')}")
    return 0


def cmd_validate(args: argparse.Namespace) -> int:
    ws = _workspace(args)
    names = args.recipes or list(loader.list_recipes(ws))
    bad = 0
    for name in names:
        try:
            loader.resolve(ws, name)
            print(f"ok      {name}")
        except loader.LoadError as e:
            bad += 1
            print(f"INVALID {name}: {e}")
    return 1 if bad else 0


def cmd_mcp(args: argparse.Namespace) -> int:
    from multiflow import mcp_server

    if args.mode == "step":
        if not args.step_dir:
            print("error: --step-dir is required for step mode", file=sys.stderr)
            return 2
        mcp_server.serve_step(Path(args.step_dir))
    else:
        mcp_server.serve_chat(_workspace(args))
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="multiflow", description="Composable multi-role agent workflows.")
    p.add_argument("-w", "--workspace", default=".", help="repository root (default: cwd)")
    sub = p.add_subparsers(dest="command", required=True)

    r = sub.add_parser("run", help="start a workflow and execute it to completion")
    r.add_argument("recipe")
    r.add_argument("task", nargs="?", default="")
    r.add_argument("--task-file", help="read the task description from a file")
    r.add_argument("-b", "--backend", default="copilot", choices=sorted(BACKENDS),
                   help="harness used for every step (fake = dry run)")
    r.add_argument("-m", "--model", action="append",
                   help="model override: MODEL for all steps or NODE=MODEL (repeatable)")
    r.add_argument("--timeout", type=int, default=None, help="per-step timeout in seconds")
    r.set_defaults(func=cmd_run)

    rs = sub.add_parser("resume", help="continue an interrupted run (latest if no id)")
    rs.add_argument("run_id", nargs="?")
    rs.add_argument("-b", "--backend", choices=sorted(BACKENDS), help="override the run's backend")
    rs.add_argument("--extra-attempt", action="store_true", help="retry a halted step once more")
    rs.add_argument("--timeout", type=int, default=None)
    rs.set_defaults(func=cmd_resume)

    s = sub.add_parser("status", help="show a run (latest if no id)")
    s.add_argument("run_id", nargs="?")
    s.add_argument("--json", action="store_true")
    s.set_defaults(func=cmd_status)

    ls = sub.add_parser("list", help="list recipes, roles or runs")
    ls.add_argument("what", nargs="?", default="recipes", choices=["recipes", "roles", "runs"])
    ls.set_defaults(func=cmd_list)

    sh = sub.add_parser("show", help="show a recipe's graph, models and permissions")
    sh.add_argument("recipe")
    sh.set_defaults(func=cmd_show)

    v = sub.add_parser("validate", help="validate recipes (all if none given)")
    v.add_argument("recipes", nargs="*")
    v.set_defaults(func=cmd_validate)

    m = sub.add_parser("mcp", help="run the MCP server (chat mode, or step mode for spawned sessions)")
    m.add_argument("mode", nargs="?", default="chat", choices=["chat", "step"])
    m.add_argument("--step-dir")
    m.set_defaults(func=cmd_mcp)
    return p


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        return args.func(args)
    except (loader.LoadError, runner.RunError, ValueError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        print("\ninterrupted; continue with `multiflow resume`", file=sys.stderr)
        return 130
