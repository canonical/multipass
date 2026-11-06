"""Schemas for roles, recipes and run state."""

from __future__ import annotations

import datetime as _dt
from typing import Literal, Optional, Union

from pydantic import BaseModel, ConfigDict, Field, model_validator

COMPLETE = "__complete__"
FAIL = "__fail__"
TERMINALS = (COMPLETE, FAIL)

ToolPreset = Literal["read-only", "edit"]

# A model may be given as a single string or per backend, e.g. {copilot: gpt-5, claude: sonnet, default: ...}
ModelSpec = Union[str, dict[str, str]]


def resolve_model_spec(spec: Optional[ModelSpec], backend: str) -> Optional[str]:
    if spec is None:
        return None
    if isinstance(spec, str):
        return spec
    return spec.get(backend) or spec.get("default")


class _Strict(BaseModel):
    model_config = ConfigDict(extra="forbid")


class RoleSpec(_Strict):
    """A persona: behaviour (markdown body), default model, tool preset and allowed verdicts."""

    name: str
    description: str = ""
    model: Optional[ModelSpec] = None
    tools: ToolPreset = "edit"
    verdicts: list[str] = Field(default_factory=lambda: ["done"])
    # Extra raw deny rules passed to a backend, e.g. {copilot: ["shell(git push)"], claude: ["Bash(git push:*)"]}
    deny_tools: dict[str, list[str]] = Field(default_factory=dict)
    body: str = ""

    @model_validator(mode="after")
    def _check(self) -> "RoleSpec":
        if not self.verdicts:
            raise ValueError(f"role '{self.name}' must declare at least one verdict")
        return self


class ArtifactSpec(_Strict):
    path: str
    description: str = ""

    @model_validator(mode="after")
    def _check(self) -> "ArtifactSpec":
        if self.path.startswith("/") or ".." in self.path.split("/"):
            raise ValueError(f"artifact path '{self.path}' must be relative and stay inside the run directory")
        return self


class NodeSpec(_Strict):
    """A step in a recipe. Only `kind: role` is executable in this version."""

    kind: Literal["role"] = "role"
    role: str
    # Artifact names; a trailing '?' marks the input as optional.
    inputs: list[str] = Field(default_factory=list)
    outputs: list[str] = Field(default_factory=list)
    model: Optional[ModelSpec] = None
    tools: Optional[ToolPreset] = None
    instructions: str = ""
    max_visits: Optional[int] = None

    @property
    def input_names(self) -> list[tuple[str, bool]]:
        return [(i.rstrip("?"), i.endswith("?")) for i in self.inputs]


class RecipeSpec(_Strict):
    name: str
    description: str = ""
    max_iterations: int = 3
    max_step_retries: int = 1
    artifacts: dict[str, ArtifactSpec] = Field(default_factory=dict)
    nodes: dict[str, NodeSpec]
    entry: str
    transitions: dict[str, dict[str, str]]

    @model_validator(mode="after")
    def _check(self) -> "RecipeSpec":
        errs: list[str] = []
        if self.entry not in self.nodes:
            errs.append(f"entry '{self.entry}' is not a node")
        for node_name, node in self.nodes.items():
            for art, _ in node.input_names:
                if art not in self.artifacts:
                    errs.append(f"node '{node_name}': unknown input artifact '{art}'")
            for art in node.outputs:
                if art not in self.artifacts:
                    errs.append(f"node '{node_name}': unknown output artifact '{art}'")
            if node_name not in self.transitions:
                errs.append(f"node '{node_name}' has no transitions")
        for src, edges in self.transitions.items():
            if src not in self.nodes:
                errs.append(f"transitions for unknown node '{src}'")
            if not edges:
                errs.append(f"node '{src}' has an empty transition map")
            for verdict, dst in edges.items():
                if dst not in self.nodes and dst not in TERMINALS:
                    errs.append(f"transition {src} --{verdict}--> '{dst}': unknown target")
        if errs:
            raise ValueError("; ".join(errs))
        return self


class ResolvedRecipe(_Strict):
    """A recipe plus the roles it uses, frozen into the run directory at start."""

    recipe: RecipeSpec
    roles: dict[str, RoleSpec]


def _now() -> str:
    return _dt.datetime.now(_dt.timezone.utc).isoformat(timespec="seconds")


class StepRecord(BaseModel):
    index: int
    node: str
    role: str
    model: Optional[str] = None
    attempt: int = 1
    started_at: str = Field(default_factory=_now)
    ended_at: Optional[str] = None
    verdict: Optional[str] = None
    summary: str = ""
    rejected: Optional[str] = None
    warnings: list[str] = Field(default_factory=list)
    exit_code: Optional[int] = None
    step_dir: str = ""


RunStatus = Literal["running", "completed", "halted", "failed"]


class RunState(BaseModel):
    run_id: str
    recipe: str
    task: str
    backend: str
    workspace: str
    status: RunStatus = "running"
    status_reason: str = ""
    current_node: Optional[str] = None
    step_index: int = 0
    attempt: int = 1
    # Set by `resume --extra-attempt`: run the current node exactly once more without counting a visit.
    bonus_attempt: bool = False
    visits: dict[str, int] = Field(default_factory=dict)
    model_overrides: dict[str, str] = Field(default_factory=dict)
    history: list[StepRecord] = Field(default_factory=list)
    pid: Optional[int] = None
    created_at: str = Field(default_factory=_now)
    updated_at: str = Field(default_factory=_now)

    def touch(self) -> None:
        self.updated_at = _now()


now_iso = _now
