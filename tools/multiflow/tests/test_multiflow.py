from __future__ import annotations

import json
import shutil
import subprocess
from pathlib import Path

import pytest

from multiflow import engine, loader, runner
from multiflow.backends import StepInvocation
from multiflow.backends.claude import ClaudeBackend
from multiflow.backends.copilot import CopilotBackend
from multiflow.backends.fake import FakeBackend
from multiflow.mcp_server import write_artifact_safely
from multiflow.step import StepContext, validate_submission, write_result


def scripted(script):
    """Build a fake agent from {node: [behaviour per visit]}; behaviour is a verdict or a callable."""
    visits: dict[str, int] = {}

    def agent(inv: StepInvocation, ctx: StepContext) -> int:
        i = visits.get(ctx.node, 0)
        visits[ctx.node] = i + 1
        steps = script[ctx.node]
        action = steps[min(i, len(steps) - 1)]
        if callable(action):
            return action(inv, ctx)
        for path in ctx.outputs.values():
            Path(path).write_text(f"{ctx.node} visit {i + 1}\n")
        write_result(inv.step_dir, action, f"{ctx.node} -> {action}")
        return 0

    return agent


def run(ws: Path, recipe: str, agent, **kw):
    run_dir = runner.create_run(ws, recipe, "the task", "fake", **kw)
    backend = FakeBackend(agent)
    state = runner.execute(run_dir, backend)
    return run_dir, state, backend


def test_review_loop_returns_to_implementer_then_completes(tmp_path):
    _, state, backend = run(tmp_path, "feature", scripted({
        "plan": ["done"], "implement": ["done"], "review": ["changes_requested", "approved"],
    }))
    assert state.status == "completed"
    assert [h.node for h in state.history] == ["plan", "implement", "review", "implement", "review"]
    assert state.visits == {"plan": 1, "implement": 2, "review": 2}
    # The second implement prompt sees the review as an existing input and the history of the rejection.
    second_impl = backend.invocations[3].prompt
    assert "changes_requested" in second_impl and "(not produced yet" not in second_impl.split("`review`")[1][:80]


def test_endless_review_loop_halts_at_max_iterations(tmp_path):
    _, state, _ = run(tmp_path, "quick_patch", scripted({"implement": ["done"], "review": ["changes_requested"]}))
    assert state.status == "halted"
    assert state.visits["implement"] == 3
    assert "max 3" in state.status_reason


def test_tampering_with_locked_plan_is_reverted_and_step_retried(tmp_path):
    def tamper(inv, ctx):
        plan = Path(ctx.locked_paths["plan"])
        with pytest.raises(PermissionError):
            plan.write_text("direct write is blocked by chmod")
        plan.unlink()  # ...but a shell can still delete + recreate, which the hash check catches
        plan.write_text("implementer rewrote the plan\n")
        Path(ctx.outputs["notes"]).write_text("notes\n")
        write_result(inv.step_dir, "done", "edited plan")
        return 0

    run_dir, state, backend = run(tmp_path, "feature", scripted({
        "plan": ["done"], "implement": [tamper, "done"], "review": ["approved"],
    }))
    assert state.status == "completed"
    rejected = [h for h in state.history if h.rejected]
    assert len(rejected) == 1 and "plan" in rejected[0].rejected and rejected[0].node == "implement"
    assert (run_dir / "artifacts" / "plan.md").read_text() == "plan visit 1\n"
    # Retry runs in its own step dir, gets the rejection as feedback, and doesn't count as a new visit.
    assert "-retry1" in str(backend.invocations[2].step_dir)
    assert "previous attempt at this step was rejected" in backend.invocations[2].prompt
    assert state.visits["implement"] == 1


def test_locked_artifacts_passed_to_backend_exclude_own_outputs(tmp_path):
    _, _, backend = run(tmp_path, "feature", scripted({"plan": ["done"], "implement": ["done"], "review": ["approved"]}))
    impl = backend.invocations[1]
    assert [p.name for p in impl.locked_paths] == ["plan.md", "review.md"]
    assert impl.tools == "edit" and backend.invocations[2].tools == "read-only"


def test_step_without_submission_is_retried_then_halts(tmp_path):
    _, state, _ = run(tmp_path, "quick_patch", scripted({"implement": [lambda inv, ctx: 1]}))
    assert state.status == "halted"
    assert len(state.history) == 2 and all("did not submit" in h.rejected for h in state.history)


def test_missing_output_rejected(tmp_path):
    def no_output(inv, ctx):
        write_result(inv.step_dir, "done", "forgot notes")
        return 0

    _, state, _ = run(tmp_path, "quick_patch", scripted({"implement": [no_output, "done"], "review": ["approved"]}))
    assert state.status == "completed"
    assert "required output 'notes'" in state.history[0].rejected


def test_resume_after_interrupt_does_not_double_count_visit(tmp_path):
    def interrupt(inv, ctx):
        raise KeyboardInterrupt

    run_dir = runner.create_run(tmp_path, "quick_patch", "t", "fake")
    with pytest.raises(KeyboardInterrupt):
        runner.execute(run_dir, FakeBackend(scripted({"implement": ["done"], "review": [interrupt]})))
    state = runner.load_state(run_dir)
    assert state.status == "running" and state.current_node == "review" and state.pid is None

    state = runner.execute(run_dir, FakeBackend(scripted({"review": ["approved"]})))
    assert state.status == "completed"
    assert state.visits == {"implement": 1, "review": 1}
    assert state.history[1].rejected.startswith("interrupted")


def test_model_precedence(tmp_path):
    resolved = loader.resolve(tmp_path, "feature")
    assert engine.resolve_model(resolved, "review", "copilot", {}) == "gpt-6.1-sol"
    assert engine.resolve_model(resolved, "review", "claude", {}) == "opus"
    assert engine.resolve_model(resolved, "review", "opencode", {}) is None
    resolved.recipe.nodes["review"].model = {"default": "node-model"}
    assert engine.resolve_model(resolved, "review", "copilot", {}) == "node-model"
    assert engine.resolve_model(resolved, "review", "copilot", {"*": "all"}) == "all"
    assert engine.resolve_model(resolved, "review", "copilot", {"*": "all", "review": "cli"}) == "cli"
    assert runner.parse_model_overrides(["x", "review=y"]) == {"*": "x", "review": "y"}


def test_unknown_model_override_node_rejected(tmp_path):
    with pytest.raises(runner.RunError):
        runner.create_run(tmp_path, "feature", "t", "fake", model_overrides={"nope": "m"})


def test_project_recipes_and_roles_override_library(tmp_path):
    (tmp_path / ".multiflow/roles").mkdir(parents=True)
    (tmp_path / ".multiflow/recipes").mkdir(parents=True)
    (tmp_path / ".multiflow/roles/reviewer.md").write_text("---\nverdicts: [lgtm, nope]\n---\nCustom reviewer\n")
    (tmp_path / ".multiflow/recipes/solo.yaml").write_text(
        "artifacts: {r: {path: r.md}}\n"
        "nodes: {check: {role: reviewer, outputs: [r]}}\n"
        "entry: check\n"
        "transitions: {check: {lgtm: __complete__, nope: __fail__}}\n"
    )
    resolved = loader.resolve(tmp_path, "solo")
    assert resolved.roles["reviewer"].body.strip() == "Custom reviewer"
    _, state, _ = run(tmp_path, "solo", scripted({"check": ["nope"]}))
    assert state.status == "failed"


def test_recipe_verdicts_must_match_role(tmp_path):
    (tmp_path / ".multiflow/recipes").mkdir(parents=True)
    (tmp_path / ".multiflow/recipes/bad.yaml").write_text(
        "nodes: {r: {role: reviewer}}\nentry: r\ntransitions: {r: {approved: __complete__, maybe: r}}\n"
    )
    with pytest.raises(loader.LoadError) as e:
        loader.resolve(tmp_path, "bad")
    assert "changes_requested" in str(e.value) and "maybe" in str(e.value)


def test_validate_submission_reports_bad_verdict_missing_output_and_tamper(tmp_path):
    locked = tmp_path / "plan.md"
    locked.write_text("plan")
    ctx = StepContext(
        run_id="r", step_index=1, attempt=1, node="n", role="x", task="t", workspace=str(tmp_path),
        step_dir=str(tmp_path), artifacts_dir=str(tmp_path), allowed_verdicts=["done"],
        outputs={"notes": str(tmp_path / "notes.md")}, inputs={},
        locked={"plan": "0" * 64}, locked_paths={"plan": str(locked)},
    )
    errors = validate_submission(ctx, "approved")
    assert len(errors) == 3


def _inv(tmp_path, tools):
    return StepInvocation(prompt="P", model="m", tools=tools, workspace=tmp_path, step_dir=tmp_path,
                          mcp_command=["py", "-m", "multiflow"], locked_paths=[tmp_path / "plan.md"])


def test_backend_commands_enforce_tool_presets(tmp_path):
    cmd, _ = CopilotBackend().command(_inv(tmp_path, "read-only"))
    assert ["--deny-tool", "write"] == cmd[cmd.index("--deny-tool"):cmd.index("--deny-tool") + 2]
    assert ["--model", "m"] == cmd[cmd.index("--model"):cmd.index("--model") + 2]
    cmd, _ = CopilotBackend().command(_inv(tmp_path, "edit"))
    assert f"write({tmp_path / 'plan.md'})" in cmd and "write" not in cmd
    assert ["--disable-mcp-server", "multiflow"] == cmd[cmd.index("--disable-mcp-server"):][:2]
    cfg = json.loads((tmp_path / "mcp-copilot.json").read_text())
    assert cfg["mcpServers"]["multiflow-step"]["args"] == ["-m", "multiflow"]

    cmd, _ = ClaudeBackend().command(_inv(tmp_path, "read-only"))
    assert "Edit" in cmd and "Write" in cmd
    cmd, _ = ClaudeBackend().command(_inv(tmp_path, "edit"))
    assert f"Edit(/{tmp_path / 'plan.md'})" in cmd and "Edit" not in cmd


def test_library_recipes_are_valid():
    ws = Path("/nonexistent-workspace")
    for name in loader.list_recipes(ws):
        loader.resolve(ws, name)


@pytest.mark.skipif(shutil.which("git") is None, reason="git not installed")
def test_read_only_role_editing_source_halts_but_new_files_only_warn(tmp_path):
    git = ["git", "-c", "user.email=t@t", "-c", "user.name=t"]
    subprocess.run(git + ["init", "-q"], cwd=tmp_path, check=True)
    (tmp_path / "src.py").write_text("x = 1\n")
    subprocess.run(git + ["add", "src.py"], cwd=tmp_path, check=True)
    subprocess.run(git + ["commit", "-qm", "init"], cwd=tmp_path, check=True)

    def review_with_by_product(inv, ctx):
        (tmp_path / "build.log").write_text("by-product")
        Path(ctx.outputs["review"]).write_text("ok")
        write_result(inv.step_dir, "approved", "ok")
        return 0

    def review_editing_source(inv, ctx):
        (tmp_path / "src.py").write_text("x = 2\n")
        return review_with_by_product(inv, ctx)

    _, state, _ = run(tmp_path, "quick_patch", scripted({"implement": ["done"], "review": [review_with_by_product]}))
    assert state.status == "completed" and "build.log" in state.history[-1].warnings[0]

    _, state, _ = run(tmp_path, "quick_patch", scripted({"implement": ["done"], "review": [review_editing_source]}))
    assert state.status == "halted" and "modified the source tree" in state.status_reason


def test_creating_a_not_yet_produced_locked_artifact_is_removed_and_rejected(tmp_path):
    def forge_review(inv, ctx):
        assert ctx.locked["review"] == ""  # absent at step start, still locked
        Path(ctx.locked_paths["review"]).write_text("VERDICT: approved (forged)")
        Path(ctx.outputs["notes"]).write_text("notes")
        write_result(inv.step_dir, "done", "forged")
        return 0

    run_dir, state, backend = run(tmp_path, "quick_patch", scripted({
        "implement": [forge_review, "done"], "review": ["approved"],
    }))
    assert "review" in state.history[0].rejected
    assert backend.invocations[0].locked_paths == [run_dir / "artifacts" / "review.md"]
    assert state.status == "completed" and state.history[1].attempt == 2


def test_write_output_refuses_symlink_escape(tmp_path):
    arts = tmp_path / "artifacts"
    (arts / "sub").parent.mkdir(parents=True)
    victim = tmp_path / "src.py"
    victim.write_text("original")
    (arts / "review.md").symlink_to(victim)
    write_artifact_safely(arts, arts / "review.md", "review")
    assert victim.read_text() == "original" and (arts / "review.md").read_text() == "review"
    (arts / "sub").symlink_to(tmp_path)
    with pytest.raises(ValueError):
        write_artifact_safely(arts, arts / "sub" / "x.md", "x")


def test_extra_attempt_runs_halted_step_exactly_once_with_feedback(tmp_path):
    from multiflow.cli import main

    failing = scripted({"implement": [lambda inv, ctx: 1]})
    run_dir, state, _ = run(tmp_path, "quick_patch", failing)
    assert state.status == "halted" and len(state.history) == 2

    import multiflow.backends as backends
    seen = []

    def still_failing(inv, ctx):
        seen.append(inv.prompt)
        return 1

    backends.BACKENDS["fake"] = lambda: FakeBackend(still_failing)
    try:
        assert main(["-w", str(tmp_path), "resume", "--extra-attempt", run_dir.name]) == 1
    finally:
        backends.BACKENDS["fake"] = FakeBackend
    state = runner.load_state(run_dir)
    assert len(seen) == 1 and "did not submit" in seen[0]
    assert state.status == "halted" and "extra attempt" in state.status_reason
    assert state.visits["implement"] == 1 and not state.bonus_attempt


def test_run_lock_prevents_concurrent_execution(tmp_path):
    run_dir = runner.create_run(tmp_path, "quick_patch", "t", "fake")

    def nested(inv, ctx):
        with pytest.raises(runner.RunError):
            runner.execute(run_dir, FakeBackend())
        assert runner.is_active(runner.load_state(run_dir), run_dir)
        return scripted({"implement": ["done"]})(inv, ctx)

    state = runner.execute(run_dir, FakeBackend(scripted({"implement": [nested], "review": ["approved"]})))
    assert state.status == "completed" and not runner.is_active(state, run_dir)


def test_unknown_recipe_suggests_closest_name(tmp_path):
    with pytest.raises(loader.LoadError, match="Did you mean 'quick_patch'"):
        loader.resolve(tmp_path, "quick_path")
