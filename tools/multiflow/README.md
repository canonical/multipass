# multiflow

Composable multi-role agent workflows (planner → implementer ⇄ reviewer, …) that run on **GitHub Copilot CLI**, **Claude Code** or **OpenCode**, and can be launched from a terminal, Copilot CLI, VS Code Copilot Chat or Claude Code.

**Full usage guide:** [dev-docs/multiflow-guide.md](../../dev-docs/multiflow-guide.md) (every command, option, integration and authoring field).

## Design

- **Roles** (`roles/*.md`) are atomic personas: prompt, default model per harness, tool preset and verdicts.
- **Recipes** (`recipes/*.yaml`) wire roles into a state machine: which artifacts each step reads/writes and where each verdict leads (e.g. `review --changes_requested--> implement`).
- **The engine, not the LLM, owns the state.** Every step runs as a fresh headless session (`copilot -p` / `claude -p` / `opencode run`) with the role's prompt, model and permissions. Roles are therefore isolated (the reviewer sees only artifacts and code, never the implementer's reasoning), each can use a different model, and permissions can be enforced per step. A single chat switching personas could do none of this, so chats only launch and monitor runs through the `multiflow` MCP server.
- **Artifacts a step doesn't own are locked** (e.g. the plan is read-only for the implementer): harness deny rules prevent writes, and a hash check after each step reverts any change and retries the step with feedback.

## Quick start

```bash
python3 -m venv ~/.venvs/multiflow && ~/.venvs/multiflow/bin/pip install -e 'tools/multiflow[test]'
ln -sf ~/.venvs/multiflow/bin/multiflow ~/.local/bin/multiflow   # must be on PATH for the MCP configs
multiflow run quick_patch "dry run" -b fake                      # no LLM calls
multiflow run feature "Add a --json flag to 'multipass networks'" -b copilot -m review=gpt-6.1-sol
```

From a chat: `/multiflow <task>` in Claude Code or VS Code, or `copilot --agent multiflow`.

## Roadmap

Node `kind` is reserved for future composition (`kind: recipe` sub-workflows, `kind: parallel` fan-out/fan-in); only `kind: role` exists today. Not yet implemented: parallel steps, sub-recipes, per-node backends, automatic recipe selection outside chats, OpenShell sandboxing.

## Development

```bash
cd tools/multiflow && python -m pytest -q     # fake backend, no LLM calls
```

Layout: `models.py` (schemas), `loader.py`, `engine.py` (pure state machine), `protection.py`, `step.py` (step contract), `prompt.py`, `runner.py`, `backends/`, `mcp_server.py`, `cli.py`, `library/` (built-in roles and recipes).
