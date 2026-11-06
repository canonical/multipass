"""Locate and parse roles (markdown + YAML frontmatter) and recipes (YAML).

Search order: <workspace>/.multiflow/{roles,recipes} first, then the built-in library,
so a project can override or extend any role/recipe by name.
"""

from __future__ import annotations

import difflib
from pathlib import Path

import yaml
from pydantic import ValidationError

from multiflow.models import ResolvedRecipe, RecipeSpec, RoleSpec

LIBRARY_DIR = Path(__file__).parent / "library"
PROJECT_DIRNAME = ".multiflow"


class LoadError(Exception):
    pass


def search_dirs(workspace: Path, kind: str) -> list[Path]:
    return [workspace / PROJECT_DIRNAME / kind, LIBRARY_DIR / kind]


def _find(workspace: Path, kind: str, name: str, suffixes: tuple[str, ...]) -> Path:
    for d in search_dirs(workspace, kind):
        for suf in suffixes:
            p = d / f"{name}{suf}"
            if p.is_file():
                return p
    known = sorted({p.stem for d in search_dirs(workspace, kind) if d.is_dir()
                    for suf in suffixes for p in d.glob(f"*{suf}")})
    close = difflib.get_close_matches(name, known, n=1)
    hint = f" Did you mean '{close[0]}'?" if close else ""
    raise LoadError(f"{kind[:-1]} '{name}' not found.{hint} Available: {', '.join(known) or 'none'} "
                    f"(searched {[str(d) for d in search_dirs(workspace, kind)]})")


def parse_role_text(text: str, default_name: str) -> RoleSpec:
    meta: dict = {}
    body = text
    if text.startswith("---"):
        parts = text.split("\n---", 1)
        if len(parts) != 2:
            raise LoadError(f"role '{default_name}': unterminated frontmatter")
        meta = yaml.safe_load(parts[0][3:]) or {}
        body = parts[1].lstrip("-").lstrip("\n")
    meta.setdefault("name", default_name)
    meta["body"] = body.strip() + "\n"
    try:
        return RoleSpec(**meta)
    except ValidationError as e:
        raise LoadError(f"role '{default_name}': {e}") from e


def load_role(workspace: Path, name: str) -> RoleSpec:
    path = _find(workspace, "roles", name, (".md",))
    return parse_role_text(path.read_text(), name)


def load_recipe(workspace: Path, name: str) -> RecipeSpec:
    path = _find(workspace, "recipes", name, (".yaml", ".yml"))
    try:
        data = yaml.safe_load(path.read_text()) or {}
        data.setdefault("name", name)
        return RecipeSpec(**data)
    except (ValidationError, yaml.YAMLError) as e:
        raise LoadError(f"recipe '{name}' ({path}): {e}") from e


def resolve(workspace: Path, recipe_name: str) -> ResolvedRecipe:
    recipe = load_recipe(workspace, recipe_name)
    roles = {n.role: load_role(workspace, n.role) for n in recipe.nodes.values()}
    errs = []
    for node_name, node in recipe.nodes.items():
        role = roles[node.role]
        routed = set(recipe.transitions.get(node_name, {}))
        verdicts = set(role.verdicts)
        if missing := verdicts - routed:
            errs.append(f"node '{node_name}': role '{role.name}' verdicts {sorted(missing)} have no transition")
        if extra := routed - verdicts:
            errs.append(f"node '{node_name}': transitions {sorted(extra)} are not verdicts of role '{role.name}'")
    if errs:
        raise LoadError(f"recipe '{recipe_name}': " + "; ".join(errs))
    return ResolvedRecipe(recipe=recipe, roles=roles)


def _list(workspace: Path, kind: str, globs: tuple[str, ...]) -> dict[str, Path]:
    found: dict[str, Path] = {}
    for d in reversed(search_dirs(workspace, kind)):  # project entries override library ones
        if d.is_dir():
            for g in globs:
                for p in sorted(d.glob(g)):
                    found[p.stem] = p
    return dict(sorted(found.items()))


def list_recipes(workspace: Path) -> dict[str, Path]:
    return _list(workspace, "recipes", ("*.yaml", "*.yml"))


def list_roles(workspace: Path) -> dict[str, Path]:
    return _list(workspace, "roles", ("*.md",))
