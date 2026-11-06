# multiflow usage guide

`multiflow` runs **multi-role agent workflows** in a repository: e.g. a *planner* writes a plan, an *implementer* changes the code, a *reviewer* or *tester* checks it, looping until approved. Every step runs as a **separate headless agent session** (GitHub Copilot CLI, Claude Code or OpenCode) with its own prompt, model and file permissions; a small state machine decides what runs next. Design rationale: [`tools/multiflow/README.md`](../tools/multiflow/README.md).

1. [Concepts](#1-concepts)
2. [Installation](#2-installation)
3. [Command-line reference](#3-command-line-reference)
4. [Choosing models](#4-choosing-models)
5. [Backends](#5-backends)
6. [Using multiflow from a chat](#6-using-multiflow-from-a-chat)
7. [MCP tool reference](#7-mcp-tool-reference)
8. [Built-in roles and recipes](#8-built-in-roles-and-recipes)
9. [Writing roles and recipes](#9-writing-roles-and-recipes)
10. [Permissions and artifact protection](#10-permissions-and-artifact-protection)
11. [Run lifecycle](#11-run-lifecycle)
12. [Run directory and Python API](#12-run-directory-and-python-api)
13. [Troubleshooting and limitations](#13-troubleshooting-and-limitations)

## 1. Concepts

| Term | Meaning |
| --- | --- |
| **Role** | A persona in a markdown file: instructions, default model, tool preset (`read-only` / `edit`) and the verdicts it may return (e.g. `reviewer`: `approved` / `changes_requested`). |
| **Recipe** | A YAML state machine: **artifacts** (files passed between steps), **nodes** (steps, each running a role), the **entry** node and **transitions** (verdict → next node). |
| **Node** | One step: which role runs, which artifacts it reads (`inputs`) and writes (`outputs`). Several nodes can use the same role. |
| **Artifact** | A file such as `plan.md` or `review.md`, stored in the run directory, not your source tree. |
| **Run** | One execution of a recipe for a task, with an id like `20261106-150000-feature-1a2b` and a directory `.multiflow/runs/<id>/`. |
| **Backend** | The agent CLI executing the steps: `copilot`, `claude`, `opencode`, or `fake` (dry run). |

A run proceeds as follows. The recipe and roles are frozen into the run directory, so later edits don't affect it. For the current node, multiflow builds a prompt (task, role instructions, inputs/outputs, history of earlier steps), locks every artifact the node doesn't own, and starts a headless session with the chosen model. The session works and calls `multiflow_submit(verdict, summary)`. multiflow then checks the verdict, the outputs and the locked artifacts: if something is wrong the step is retried with feedback, otherwise the verdict's transition selects the next node. The run ends `completed` (reached `__complete__`), `failed` (reached `__fail__`) or `halted` (a limit or error stopped it; resumable).

In every chat integration the chat is only the **launcher and monitor**; the work is done by the step sessions, which is what lets each step have its own model and permissions.

## 2. Installation

Requires Python 3.10+ and at least one logged-in agent CLI on `PATH`: `copilot`, `claude` or `opencode`.

```bash
python3 -m venv ~/.venvs/multiflow
~/.venvs/multiflow/bin/pip install -e tools/multiflow          # "tools/multiflow[test]" adds pytest
ln -sf ~/.venvs/multiflow/bin/multiflow ~/.local/bin/multiflow  # or activate the venv before starting your editor
multiflow validate && multiflow run quick_patch "dry run" -b fake   # check the install without LLM calls
```

`multiflow` **must be on `PATH`** for the chat integrations, because `.mcp.json` and `.vscode/mcp.json` start the server with the bare command `multiflow`. Install with `-e` (editable) so the tool follows changes in the repository. Run the tests from `tools/multiflow` with `python -m pytest -q`.

## 3. Command-line reference

```
multiflow [-w WORKSPACE] <command> [options]
```

`-w, --workspace DIR` (default: current directory, must precede the command) is the repository root: steps run there, project roles/recipes are read from `DIR/.multiflow/`, and runs are stored in `DIR/.multiflow/runs/`.

Exit codes: `0` success / run completed; `1` run `halted` or `failed`, or `validate` found invalid recipes; `2` usage or configuration error (unknown recipe, bad `--model`, backend not installed, run not found or already being executed); `130` Ctrl-C.

### `run`: start a workflow

```
multiflow run RECIPE [TASK] [--task-file FILE] [-b BACKEND] [-m MODEL | -m NODE=MODEL]... [--timeout SECONDS]
```

| Option | Default | Meaning |
| --- | --- | --- |
| `RECIPE` | required | Recipe name (`multiflow list`). Unknown names get a "did you mean" suggestion. |
| `TASK` / `--task-file FILE` | one required | Task description, inline (quoted) or from a file for long, multi-line tasks. The file wins if both are given. |
| `-b, --backend` | `copilot` | Harness for every step: `copilot`, `claude`, `opencode`, `fake`. |
| `-m, --model` | none | Repeatable. `-m MODEL` for all steps, `-m NODE=MODEL` for one node ([§4](#4-choosing-models)). Unknown nodes are rejected. |
| `--timeout SECONDS` | none | Wall-clock limit per step session; a timeout counts as a failed attempt. |

The run executes in the foreground, prints one line per step, then lists the artifacts. After Ctrl-C, continue with `resume`.

```bash
multiflow run quick_patch "Fix the typo in the 'multipass list' help text"
multiflow run bugfix --task-file issue-1234.md -b claude
multiflow run feature "Add a --json flag to 'multipass networks'" -m claude-sonnet-5.5 -m review=gpt-6.1-sol --timeout 1800
multiflow -w ~/src/other-repo run quick_patch "Bump the version"
```

### `resume`: continue a run

```
multiflow resume [RUN_ID] [-b BACKEND] [--extra-attempt] [--timeout SECONDS]
```

`RUN_ID` defaults to the most recently updated run. `-b` switches the harness from now on (e.g. if one is rate-limited). `--timeout` applies to the resumed steps. Typical uses:

- **Interrupted run** (killed terminal, reboot, or a chat-started run shown as `stopped`): `multiflow resume <id>` re-runs the interrupted step without counting a new loop iteration.
- **Halted run**: fix the cause (expired login, broken environment, too low a limit), then `multiflow resume --extra-attempt <id>`. The current step gets **exactly one** more try, with the previous rejection as feedback and without counting a visit; if rejected again, the run halts again.

Resuming a `completed`/`failed` run only prints its status; resuming a run another process is executing fails with "already being executed".

### `status`, `list`, `show`, `validate`

| Command | What it does |
| --- | --- |
| `multiflow status [RUN_ID] [--json]` | Recipe, backend, status and reason, current node, every step (node, role, model, verdict or rejection, summary, warnings) and the artifacts so far. Defaults to the latest run. `--json` gives the same object as the `multiflow_status` MCP tool. |
| `multiflow list [recipes\|roles\|runs]` | Recipes (default) or roles with description and source file (project files override built-ins; broken ones show `INVALID: …`), or all runs with status and task. |
| `multiflow show RECIPE` | Limits, entry node and, per node: role, tool preset, model, inputs, outputs, the artifacts **read-only** for it, and its transitions. Use it to check permissions and models before running. |
| `multiflow validate [RECIPE...]` | Checks all (or the named) recipes: schema, unknown fields, references, entry, targets, and that each role verdict is routed and each route is a real verdict. Exit 1 if any is invalid. |
| `multiflow mcp [chat]` | The chat MCP server over stdio ([§6](#6-using-multiflow-from-a-chat)). `multiflow mcp step --step-dir DIR` is the per-step server multiflow starts itself. |

## 4. Choosing models

Each step runs with `--model <id>`, taken from the **first** of these that is set:

1. Run override for that node: CLI `-m NODE=MODEL`, MCP `models: {"NODE": "MODEL"}`.
2. Run override for all nodes: CLI `-m MODEL`, MCP `models: {"*": "MODEL"}`.
3. The node's `model:` in the recipe.
4. The role's `model:` in its frontmatter.
5. The harness default (no `--model` flag passed).

Levels 3–4 accept a string or a per-harness map, e.g. `model: {copilot: gpt-6.1-sol, claude: opus, default: x}` (`default` covers other backends). Run overrides are plain strings passed as-is, so they must be valid for the run's harness: Copilot ids like `claude-opus-5.5`, `claude-sonnet-5.5`, `claude-haiku-4.5`, `gpt-6.1-sol` (see `copilot help config`); Claude aliases `opus`, `sonnet`, `haiku` or full names; OpenCode `provider/model`.

Overrides are saved in the run, so `resume` keeps them. `multiflow show` displays levels 3–4 and `multiflow status` the model each step actually used. The built-in library uses a different model family for the reviewer than for the implementer, for a more independent review.

## 5. Backends

One backend runs all steps of a run (`resume -b` can switch it). Each step's command line (without the prompt) is saved in `steps/<step>/command.txt`, its output in `log.txt`.

| Backend | Command per step | Permission rules | Step MCP config |
| --- | --- | --- | --- |
| `copilot` | `copilot -p <prompt> --allow-all-tools --no-ask-user -s` | read-only roles: `--deny-tool write`; edit roles: `--deny-tool write(<path>)` per locked artifact | `--additional-mcp-config`; the chat server is disabled with `--disable-mcp-server multiflow` |
| `claude` | `claude -p <prompt> --permission-mode bypassPermissions` | read-only roles: `--disallowedTools Edit Write MultiEdit NotebookEdit`; edit roles: those tools denied per locked artifact | `--mcp-config` |
| `opencode` (untested) | `opencode run [--model M] <prompt>` | read-only roles: `permission.edit = deny` | generated config via `OPENCODE_CONFIG` |
| `fake` | in-process | — | — (writes placeholder outputs and submits each role's first verdict) |

Roles can add raw deny rules per backend with `deny_tools` ([§9](#9-writing-roles-and-recipes)). Every step session gets the `MULTIFLOW_STEP_DIR` environment variable, and the chat server refuses to start workflows when it is set, which prevents nested workflows.

## 6. Using multiflow from a chat

The repository has ready-made configuration for each harness; all of it starts the server with `multiflow`, which must be on `PATH` ([§2](#2-installation)). After installing, **restart the harness** so it loads the server.

| Harness | Server config | How to invoke |
| --- | --- | --- |
| Claude Code | `.mcp.json` (approve on first use; check with `claude mcp list`, or add with `claude mcp add multiflow -- multiflow mcp`) | `/multiflow [recipe] <task>` (`.claude/commands/multiflow.md`, uses `backend: "claude"`) |
| VS Code Copilot Chat | `.vscode/mcp.json` (start/trust it from the MCP view) | `/multiflow [recipe] <task>` in Agent mode (`.github/prompts/multiflow.prompt.md`), or pick the `multiflow` agent (`.github/agents/multiflow.agent.md`); both use `backend: "copilot"` |
| Copilot CLI | `.mcp.json` (check with `copilot mcp list` or `/mcp`), or `~/.copilot/mcp-config.json` | `copilot --agent multiflow [-p "<task>"]`, or ask any session to "start an multiflow bugfix run for …" |
| OpenCode (untested) | `opencode.json`: `{"mcp": {"multiflow": {"type": "local", "command": ["multiflow", "mcp"], "enabled": true}}}` | ask for an multiflow run with `backend: "opencode"` |
| Any MCP client | `multiflow -w /path/to/repo mcp` over stdio (without `-w`, the working directory is the workspace) | call the tools in [§7](#7-mcp-tool-reference) |

```
/multiflow bugfix The VM list shows stale IPs after a restart
/multiflow Add a --format=yaml option to multipass info      # recipe picked automatically
```

The chat lists recipes, uses the one you named (or picks the best fit and says why), calls `multiflow_start`, reports the run id, polls `multiflow_status` about once a minute, and finally summarizes the steps, reads the artifacts and shows the diff. If the run halts it explains why and suggests `resume --extra-attempt`. To pick models, say so ("use opus for the review"); the chat passes them as `models`.

Chat-started runs execute **in the background**, detached from the chat: closing the chat doesn't stop them, `multiflow status` follows them from any terminal, and their runner output goes to `.multiflow/runs/<id>/runner.log`.

## 7. MCP tool reference

**Chat server** (`multiflow mcp`):

| Tool | Parameters | Returns |
| --- | --- | --- |
| `multiflow_list_recipes` | — | `{name: {description, nodes: {node: role}, entry}}`, or `{name: {error}}` for invalid recipes. |
| `multiflow_start` | `recipe`, `task` (full description), `backend` (default `copilot`), `models` (optional `{"*": m}` / `{node: m}`) | `{run_id, run_dir, hint}` once the background runner has started, or `{error}`. |
| `multiflow_status` | `run_id` (empty = latest) | Same as `status --json`: `run_id, recipe, backend, task, status, status_reason, current_node, history[], artifacts, run_dir`; history entries have `index, node, role, model, attempt, verdict, summary, rejected, warnings, ended_at`. |
| `multiflow_list_runs` | `limit` (default 10) | Newest first: `[{run_id, recipe, status, task, updated_at}]`. |
| `multiflow_read_artifact` | `run_id` (empty = latest), `name` (e.g. `plan`, `review`) | Artifact text (up to 100 000 characters), a "not produced yet" note, or `ERROR: …`. |

**Step server** (`multiflow-step`, injected into every step session; relevant when writing roles or debugging):

| Tool | Parameters | Behaviour |
| --- | --- | --- |
| `multiflow_step_info` | — | Task, node, role, attempt, allowed verdicts, inputs, outputs, read-only artifacts, whether the source tree is read-only. |
| `multiflow_write_output` | `name`, `content` | Writes one of the step's declared outputs; this is how **read-only** roles produce them. Rejects other names, never follows symlinks, writes only inside the run's artifacts directory. |
| `multiflow_submit` | `verdict`, `summary` | Validates the verdict, that outputs exist and are non-empty, and that locked artifacts are intact. Returns `NOT ACCEPTED` with problems to fix in the same session, or `ACCEPTED`. |

If MCP is unavailable inside a step, the agent may instead write `steps/<step>/result.json` (`{"verdict": "...", "summary": "..."}`), which is validated the same way.

## 8. Built-in roles and recipes

| Role | Tools | Verdicts | Model (copilot / claude) | Does |
| --- | --- | --- | --- | --- |
| `planner` | read-only | `done` | `claude-opus-5.5` / `opus` | Investigates the code, writes a plan with validation steps. |
| `implementer` | edit | `done` | `claude-sonnet-5.5` / `sonnet` | Implements the plan (or task), validates, writes notes; addresses review/test feedback. |
| `reviewer` | read-only | `approved`, `changes_requested` | `gpt-6.1-sol` / `opus` | Independently reviews the actual diff against task and plan. |
| `tester` | read-only | `passed`, `failed` | `claude-sonnet-5.5` / `sonnet` | Runs builds/tests and reports with evidence. |

| Recipe | Flow | Artifacts | Use for |
| --- | --- | --- | --- |
| `quick_patch` | `implement` → `review`; `changes_requested` → `implement` | `notes`, `review` | Small, well-understood changes. |
| `bugfix` | `diagnose` (planner) → `fix` → `verify` (tester); `failed` → `fix` | `plan`, `notes`, `test_report` | Defects: root cause first, then a test-verified fix. |
| `feature` | `plan` → `implement` → `review`; `changes_requested` → `implement` | `plan`, `notes`, `review` | New functionality with a plan and independent review. |

All use `max_iterations: 3` and `max_step_retries: 1`. Sources: `tools/multiflow/multiflow/library/`.

## 9. Writing roles and recipes

Project roles go in `<repo>/.multiflow/roles/<name>.md` and recipes in `<repo>/.multiflow/recipes/<name>.yaml` (or `.yml`). A project file with the same name as a built-in **replaces** it for this repository, including inside built-in recipes. Check with `multiflow validate` and `multiflow show`, then try with `-b fake`. Unknown fields are an error in both formats.

### Roles

```markdown
---
name: security_reviewer                 # optional, defaults to the file name
description: Reviews changes for security problems only.
model: {copilot: gpt-6.1-sol, claude: opus}
tools: read-only
verdicts: [secure, issues_found]
deny_tools:
  copilot: ["shell(git push)", "shell(curl:*)"]
  claude: ["Bash(git push:*)", "Bash(curl:*)"]
---
You are a security reviewer. Inspect the working-tree diff for injection, path traversal, ...
Write your findings to the review output; return `secure` only if there is nothing to fix.
```

| Field | Default | Meaning |
| --- | --- | --- |
| `name` | file name | Name used by recipes. |
| `description` | `""` | Shown by `list roles`. |
| `model` | none | String or `{backend: model, default: model}` ([§4](#4-choosing-models), level 4). |
| `tools` | `edit` | `read-only`: no file-write tools, outputs only via `multiflow_write_output`, source-tree changes halt the run. `edit`: may edit the source tree but not locked artifacts. Nodes can override it. |
| `verdicts` | `[done]` | Allowed results; every recipe using the role must route each one. |
| `deny_tools` | `{}` | Extra raw deny rules per backend (`--deny-tool` for copilot, `--disallowedTools` for claude). |

The markdown body only needs to describe the persona and what a good result looks like: multiflow adds the task, inputs/outputs, read-only artifacts, step history, rejection feedback and submit instructions. Refer to outputs by purpose rather than by path.

### Recipes

```yaml
name: secure_feature                      # optional, defaults to the file name
description: Plan, implement, then functional and security review.
max_iterations: 4                         # each node may run at most 4 times
max_step_retries: 1                       # retries of a rejected step before halting

artifacts:
  plan:     {path: plan.md, description: implementation plan}
  notes:    {path: implementation_notes.md}
  review:   {path: review.md}
  security: {path: reviews/security.md}   # sub-directories are allowed

nodes:
  plan:
    role: planner
    outputs: [plan]
    model: claude-opus-5.5                # node-level model (level 3)
  implement:
    role: implementer
    inputs: [plan, "review?", "security?"]   # '?' = optional input; quote it in flow lists
    outputs: [notes]
  review:
    role: reviewer
    inputs: [plan, notes]
    outputs: [review]
    instructions: Focus on correctness and tests; ignore security, it is reviewed separately.
  security:
    role: security_reviewer
    inputs: [notes]
    outputs: [security]
    max_visits: 2
  give_up:
    role: reviewer
    inputs: [notes]
    outputs: [review]
    tools: read-only

entry: plan
transitions:
  plan:      {done: implement}
  implement: {done: review}
  review:    {approved: security, changes_requested: implement}
  security:  {secure: __complete__, issues_found: implement}
  give_up:   {approved: __complete__, changes_requested: __fail__}
```

| Recipe field | Default | Meaning |
| --- | --- | --- |
| `name`, `description` | file name, `""` | Shown by `list`/`show` and used by chats to choose a recipe. |
| `max_iterations` | `3` | Maximum runs of **each** node; exceeding it halts the run. |
| `max_step_retries` | `1` | Retries (with feedback) of a rejected step before halting. |
| `artifacts` | `{}` | Name → `{path, description}`; `path` is relative to the run's `artifacts/` (no absolute paths or `..`). |
| `nodes`, `entry` | required | Steps and the first step. |
| `transitions` | required | Node → `{verdict: target}` for every node; targets are nodes, `__complete__` or `__fail__`. |

| Node field | Default | Meaning |
| --- | --- | --- |
| `role` | required | Role to run. |
| `inputs` | `[]` | Artifacts to read; `name?` marks one as optional (e.g. the review on the first iteration). Inputs are read-only. |
| `outputs` | `[]` | Artifacts the step must write (non-empty). **All other artifacts are locked** for it. |
| `model` | none | String or per-backend map ([§4](#4-choosing-models), level 3). |
| `tools` | role's | Overrides the role's tool preset. |
| `instructions` | `""` | Extra instructions appended to the role body. Reusing a role with per-node instructions usually beats copying it. |
| `max_visits` | `max_iterations` | Per-node loop limit. |
| `kind` | `role` | Reserved for future node kinds (sub-recipes, parallel groups). |

An artifact written by several nodes (like `review` above) is overwritten each time, so readers see the latest; use separate artifacts when a later step needs both. Use `__fail__` for explicit "cannot be done" outcomes, and loops bounded by `max_iterations` for "try again".

## 10. Permissions and artifact protection

In every step, the node's **outputs** are writable; **every other artifact is locked**, including ones that don't exist yet (in `feature`, the implementer can neither change `plan.md` nor create `review.md`); and **read-only roles** must not modify the source tree. This is enforced in layers:

1. **Harness deny rules** ([§5](#5-backends)): read-only roles get no write tools and write outputs through `multiflow_write_output`; edit roles are denied writes to locked artifact paths.
2. **chmod a-w** on locked artifacts during the step (best effort; ineffective on some network/FUSE filesystems).
3. **Hash verification** (authoritative): after the step, locked artifacts are compared with a snapshot. Any change, deletion, creation or replacement with a symlink is **reverted**, and the step is **rejected and retried** with the reason as feedback. `multiflow_submit` runs the same checks, so the agent can fix problems before finishing.
4. **Source-tree guard for read-only roles** (git repositories only): modifying tracked files **halts** the run without reverting (inspect with `git status`); new untracked files such as build outputs only produce a warning.

**Security note:** apart from these rules, steps run with all tools allowed (`--allow-all-tools` / `bypassPermissions`), including arbitrary shell commands, so a determined agent can still damage the source tree. Use a disposable checkout, worktree, container or VM for untrusted tasks, and review the diff before committing.

## 11. Run lifecycle

| Status | Meaning | Next |
| --- | --- | --- |
| `running` | A runner is executing it. | Wait or poll. |
| `stopped` | Shown when the state says `running` but no runner holds the run (crash, killed terminal, reboot). | `multiflow resume <id>` |
| `completed` | Reached `__complete__`. | Review the diff and artifacts. |
| `failed` | Reached `__fail__`. | Read `status_reason` and the last artifacts. |
| `halted` | A limit or error stopped it. | Fix the cause, then `multiflow resume --extra-attempt <id>`. |

- **Rejected steps** (no submission, invalid verdict, missing or empty output, locked artifact touched, timeout, harness crash) are retried up to `max_step_retries` times with the reason added to the prompt, then the run halts. Retries don't count as loop iterations.
- **Loops**: each time a node starts (excluding retries) its visit count grows; if it would exceed `max_visits`/`max_iterations` the run halts instead of looping forever. Before `--extra-attempt` you can raise the frozen limit in the run's `recipe.json`.
- A **read-only role modifying tracked files** also halts the run.
- **One runner per run**: a lock file ensures that a second `resume` of an active run fails cleanly.

## 12. Run directory and Python API

```
<workspace>/.multiflow/
  roles/ recipes/       # optional project roles and recipes
  runs/<run-id>/        # git-ignored; delete a directory to delete the run
    state.json          # status, current node, visits, model overrides, step history
    recipe.json         # frozen recipe + roles
    runner.lock         # held while a runner executes the run
    runner.log          # output of background (chat-started) runners
    artifacts/          # plan.md, implementation_notes.md, review.md, ...
    steps/01-plan/      # one directory per step; retries get -retryN (e.g. 04-implement-retry1)
      step.json         # step context: task, role, inputs, outputs, locked artifacts + hashes
      prompt.md         # full prompt sent to the session   <- first place to look when debugging
      command.txt       # harness command line (prompt elided)
      log.txt           # full session output                <- and this
      result.json       # submitted verdict and summary
      mcp-*.json        # generated MCP config
      locked-snapshot/  # copies used to restore tampered artifacts
```

```python
from pathlib import Path
from multiflow import runner
from multiflow.backends import get_backend

run_dir = runner.create_run(Path("/path/to/repo"), "feature", "Add a --json flag", backend="copilot",
                            model_overrides={"*": "claude-sonnet-5.5", "review": "gpt-6.1-sol"})
state = runner.execute(run_dir, get_backend("copilot"), timeout=1800, log=print)
print(state.status, state.status_reason)
```

For tests, `multiflow.backends.fake.FakeBackend(agent)` takes a callable `agent(invocation, step_context) -> exit_code` that plays the agent: it writes the outputs and calls `multiflow.step.write_result(step_dir, verdict, summary)`. See `tools/multiflow/tests/test_multiflow.py`.

## 13. Troubleshooting and limitations

| Symptom | Cause / fix |
| --- | --- |
| Chat lacks the multiflow tools; Copilot log says `failed to spawn MCP server process: No such file or directory` | `multiflow` isn't on the harness's `PATH`: link it into `~/.local/bin` ([§2](#2-installation)) and restart the harness. |
| Claude Code doesn't load the server | Approve the project server (`claude mcp list`, or restart and accept). |
| `recipe 'x' not found. Did you mean 'y'?` | Typo in the recipe name; the error lists the available ones. |
| `backend 'copilot' not available` | The harness CLI isn't on `PATH`. |
| Changes in the repository have no effect | multiflow was installed without `-e`; reinstall with `pip install -e tools/multiflow`. |
| Step rejected: `did not submit a result via multiflow_submit` | The session ended without submitting (harness error, expired login, timeout); read the step's `log.txt`. |
| Step rejected: `modified locked artifacts [...] (reverted)` | The role touched an artifact it doesn't own; the retry usually succeeds, otherwise tighten the role's instructions. |
| Halted: `node 'x' would run N times` | Loop limit reached (e.g. the reviewer keeps requesting changes). Read the last review; raise the limit and `resume --extra-attempt`. |
| Halted: `read-only role ... modified the source tree` | Inspect and clean up `git status`/`git diff`, then `resume --extra-attempt`. |
| Status `stopped` / `already being executed` | No runner owns the run (`resume` it) / another terminal or chat-started runner does (wait or stop it). |
| `--model override for unknown node`, or a model error in a step's log | Node name not in the recipe (see `show`) / model id invalid for that harness ([§4](#4-choosing-models)). |
| Recipe `INVALID` | `multiflow validate NAME` shows why; quote `"x?"` optional inputs in YAML flow lists. |

**Limitations:** one backend per run; only `kind: role` nodes (no parallel fan-out or sub-recipes yet); automatic recipe selection only in chats; steps run sequentially and can take minutes each; no sandboxing beyond the harness permission rules; OpenCode is untested and OpenShell unsupported.
