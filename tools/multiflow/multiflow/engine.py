"""Pure state-machine logic: no I/O, no subprocesses."""

from __future__ import annotations

from typing import Optional

from multiflow.models import COMPLETE, FAIL, ResolvedRecipe, RunState, ToolPreset, resolve_model_spec


def resolve_model(resolved: ResolvedRecipe, node_name: str, backend: str, overrides: dict[str, str]) -> Optional[str]:
    """CLI override (per node, then '*') > recipe node > role > backend default (None)."""
    if node_name in overrides:
        return overrides[node_name]
    if "*" in overrides:
        return overrides["*"]
    node = resolved.recipe.nodes[node_name]
    return resolve_model_spec(node.model, backend) or resolve_model_spec(resolved.roles[node.role].model, backend)


def resolve_tools(resolved: ResolvedRecipe, node_name: str) -> ToolPreset:
    node = resolved.recipe.nodes[node_name]
    return node.tools or resolved.roles[node.role].tools


def allowed_verdicts(resolved: ResolvedRecipe, node_name: str) -> list[str]:
    return list(resolved.roles[resolved.recipe.nodes[node_name].role].verdicts)


def start(resolved: ResolvedRecipe, state: RunState) -> None:
    state.current_node = resolved.recipe.entry
    state.status = "running"


def enter_node(resolved: ResolvedRecipe, state: RunState) -> Optional[str]:
    """Count a visit to the current node. Returns a halt reason if the visit cap is exceeded."""
    node_name = state.current_node
    assert node_name is not None
    node = resolved.recipe.nodes[node_name]
    cap = node.max_visits or resolved.recipe.max_iterations
    visits = state.visits.get(node_name, 0) + 1
    if visits > cap:
        return f"node '{node_name}' would run {visits} times (max {cap}); halting to avoid an endless loop"
    state.visits[node_name] = visits
    return None


def advance(resolved: ResolvedRecipe, state: RunState, verdict: str) -> None:
    node_name = state.current_node
    assert node_name is not None
    edges = resolved.recipe.transitions[node_name]
    if verdict not in edges:
        raise ValueError(f"verdict '{verdict}' not allowed for node '{node_name}' (allowed: {sorted(edges)})")
    target = edges[verdict]
    state.attempt = 1
    if target == COMPLETE:
        state.status, state.current_node, state.status_reason = "completed", None, f"{node_name} -> {verdict}"
    elif target == FAIL:
        state.status, state.current_node, state.status_reason = "failed", None, f"{node_name} -> {verdict}"
    else:
        state.current_node = target
