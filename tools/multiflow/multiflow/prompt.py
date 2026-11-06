"""Assemble the prompt for a single headless step session."""

from __future__ import annotations

import json

from multiflow.models import ResolvedRecipe, RunState
from multiflow.step import StepContext

HISTORY_LIMIT = 8


def build_prompt(resolved: ResolvedRecipe, state: RunState, ctx: StepContext, feedback: str = "") -> str:
    recipe = resolved.recipe
    node = recipe.nodes[ctx.node]
    role = resolved.roles[node.role]
    out: list[str] = []

    out.append(f"# multiflow step: `{ctx.node}` (role: {role.name})\n")
    out.append(
        "You are one step in a multi-role workflow driven by an external state machine. "
        "Do ONLY this role's job, then report back. Other roles run in separate sessions after you; "
        "communicate with them exclusively through the artifact files listed below.\n"
    )

    out.append("## Task\n")
    out.append(state.task.strip() + "\n")
    if recipe.description:
        out.append(f"Workflow: **{recipe.name}** - {recipe.description.strip()}\n")

    out.append("## Your role\n")
    out.append(role.body.strip() + "\n")
    if node.instructions:
        out.append("### Additional instructions for this step\n")
        out.append(node.instructions.strip() + "\n")

    out.append("## Artifacts\n")
    out.append(f"Workspace (repository root): `{ctx.workspace}`\n")
    if ctx.inputs:
        out.append("Inputs (read these first):\n")
        for name, inp in ctx.inputs.items():
            desc = recipe.artifacts[name].description
            state_note = "" if inp.exists else (" (not produced yet - ignore)" if inp.optional else " (MISSING)")
            out.append(f"- `{name}`: `{inp.path}`{state_note}{' - ' + desc if desc else ''}")
        out.append("")
    if ctx.outputs:
        out.append("Outputs you MUST write (non-empty) before submitting:\n")
        for name, path in ctx.outputs.items():
            desc = recipe.artifacts[name].description
            out.append(f"- `{name}`: `{path}`{' - ' + desc if desc else ''}")
        how = ("Write them with the MCP tool `multiflow_write_output(name, content)` (your file-editing tools "
               "are disabled)." if ctx.read_only_workspace else
               "Write them with your file tools or the MCP tool `multiflow_write_output(name, content)`.")
        out.append(how + "\n")
    if ctx.locked_paths:
        out.append("Read-only artifacts (modifying them gets your step rejected and reverted):\n")
        for name, path in ctx.locked_paths.items():
            out.append(f"- `{name}`: `{path}`")
        out.append("")
    if ctx.read_only_workspace:
        out.append(
            "**You are READ-ONLY for the source tree.** Do not create, edit or delete files outside your "
            "output artifacts. Running builds/tests is fine as long as they don't modify tracked files.\n"
        )

    previous = [h for h in state.history if h.verdict or h.rejected][-HISTORY_LIMIT:]
    if previous:
        out.append("## Workflow history so far\n")
        for h in previous:
            outcome = f"verdict `{h.verdict}`" if h.verdict else f"REJECTED ({h.rejected})"
            summary = f": {h.summary.strip()}" if h.summary else ""
            out.append(f"- step {h.index} `{h.node}` ({h.role}) -> {outcome}{summary}")
        out.append("")

    if feedback:
        out.append("## Your previous attempt at this step was rejected\n")
        out.append(feedback.strip() + "\nFix this in this attempt.\n")

    verdicts = " | ".join(f"`{v}`" for v in ctx.allowed_verdicts)
    out.append("## Finishing the step (required)\n")
    out.append(
        f"When done, call the MCP tool `multiflow_submit` with `verdict` (one of: {verdicts}) and a short "
        "`summary` (what you did / found, which the next roles will see). If it returns errors, fix them and "
        "call it again. Stop working after a successful submit.\n"
    )
    out.append(
        f"Only if the `multiflow_submit` tool is unavailable, write this JSON to `{ctx.result_path}` instead:\n"
    )
    out.append("```json\n" + json.dumps({"verdict": ctx.allowed_verdicts[0], "summary": "..."}) + "\n```\n")
    return "\n".join(out)
