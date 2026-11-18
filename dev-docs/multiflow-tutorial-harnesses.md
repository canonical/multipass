# Tutorial: using multiflow from each harness

This tutorial walks through running a workflow from GitHub Copilot CLI, Claude Code, VS Code Copilot Chat, OpenCode, a plain terminal and any other MCP client. Every option is listed in the [usage guide](multiflow-guide.md); this page shows the typical path for each harness.

**How it fits together.** The chat only **launches and monitors** a run. It calls `multiflow_start`, which spawns a background `multiflow` runner process. That runner is the orchestrator: it walks the recipe and starts every step as a fresh headless session (`copilot -p`, `claude -p` or `opencode run`) with the step's role prompt, model and file permissions. By default the steps use the **same harness you launched from** ([guide §5](multiflow-guide.md#5-backends)), so a run started in Claude Code runs its steps with `claude`. The chat polls `multiflow_status` and summarizes the result at the end. Runs live in `<repo>/.multiflow/runs/`, so every harness and terminal sees the same runs.

## 0. One-time setup (all harnesses)

1. Install multiflow and put it on `PATH` ([guide §2](multiflow-guide.md#2-installation)). The harness starts the MCP server with the bare command `multiflow`, so it must be found from the environment the harness runs in.
2. Check the installation without any LLM calls, from the repository you want to work on:

   ```bash
   cd ~/src/my-repo
   multiflow validate
   multiflow run quick_patch "dry run" -b fake
   ```

3. Copy the integration files for your harnesses from this repository into the target repository (or register the server user-wide, described per harness below):

   | File | Used by |
   | --- | --- |
   | `.mcp.json` | Claude Code, Copilot CLI |
   | `.claude/commands/multiflow.md` | Claude Code (`/multiflow`), Copilot CLI (loaded as the `/multiflow` skill) |
   | `.vscode/mcp.json` | VS Code |
   | `.github/prompts/multiflow.prompt.md` | VS Code (`/multiflow`) |
   | `.github/agents/multiflow.agent.md` | VS Code agent picker, Copilot CLI `--agent multiflow` |

4. Make sure the harness that will execute the steps is installed and logged in. It is the harness you launch from unless you ask for another backend. VS Code is the special case: its steps run with the `copilot` CLI, so that CLI must be installed and logged in too.
5. Add `.multiflow/runs/` to the target repository's `.gitignore`.

Restart the harness after changing any of these files: MCP servers, commands and agents are loaded at startup.

## 1. GitHub Copilot CLI

**Register the server.** Copilot CLI reads `.mcp.json` from the repository root. To make multiflow available in every repository instead, add it to `~/.copilot/mcp-config.json`:

```json
{
  "mcpServers": {
    "multiflow": {"type": "local", "command": "multiflow", "args": ["mcp"], "tools": ["*"]}
  }
}
```

Without `-w`, the server works on the directory where you started `copilot`. Check that it loaded with `copilot mcp list` in a shell, or `/mcp` inside a session. The tools appear as `multiflow-multiflow_start`, `multiflow-multiflow_status` and so on.

**Start a run.** Inside an interactive `copilot` session, use any of these:

```
/multiflow bugfix The VM list shows stale IPs after a restart
/multiflow Add a --format=yaml option to the info command
Start a multiflow feature run to add a --json flag to the status command, and use opus for the review.
```

The first form names the recipe. The second lets the chat pick one and explain why. The third shows that plain language works too, including model requests. You can also start a dedicated launcher session with `copilot --agent multiflow`, or run it headless:

```bash
copilot --agent multiflow --allow-all-tools -p "bugfix: the VM list shows stale IPs after a restart"
```

`--allow-all-tools` lets the headless launcher call the multiflow tools and `sleep` between polls without asking. The session ends after the run finishes and it has printed the summary.

**What you see.** The chat lists the recipes, calls `multiflow_start` and reports something like `run 20261116-150000-bugfix-1a2b started (backend: copilot, detected from MCP client 'copilot-cli')`. Every minute it calls `multiflow_status`, then summarizes each step's verdict, reads the final artifacts and shows `git diff --stat`.

**Autocomplete.** Copilot CLI completes `/multiflow` and shows its argument hint `[quick_patch|bugfix|feature|pr_split] <task>`. It does not request MCP prompts or completions, so the per-recipe prompts described for VS Code don't appear here.

## 2. Claude Code

**Register the server.** Claude Code reads the project `.mcp.json` and asks you to approve it on first use. Check with `claude mcp list`. Alternatively, add it yourself:

```bash
claude mcp add multiflow -- multiflow mcp              # this project
claude mcp add -s user multiflow -- multiflow mcp      # every project
```

**Start a run** with the `/multiflow` command from `.claude/commands/multiflow.md`:

```
/multiflow bugfix The VM list shows stale IPs after a restart
/multiflow feature Add a --json flag to the status command; use sonnet for the implementation
```

Headless, for scripts:

```bash
claude -p "/multiflow quick_patch Fix the typo in the --help output of the list command" --permission-mode bypassPermissions
```

The steps run with `claude` (detected from the `claude-code` MCP client name). Model names are Claude aliases (`opus`, `sonnet`, `haiku`) or full model names.

**MCP prompts.** The server's prompts appear as `/mcp__multiflow__<recipe>` and `/mcp__multiflow__run`. Claude Code splits prompt arguments on spaces and ignores quotes, though, so a multi-word task reaches the prompt truncated to its first word. The chat usually reconstructs the task from your message, but `/multiflow` passes the whole text reliably, so prefer it.

## 3. VS Code Copilot Chat

**Register the server.** `.vscode/mcp.json` starts `multiflow --workspace ${workspaceFolder} mcp`, so the server always works on the open folder. Open the Command Palette, run **MCP: List Servers**, select `multiflow` and start it, trusting it when asked. When the server starts, the Chat view's tools list (the tools icon in Agent mode) shows the five `multiflow_*` tools.

Two prerequisites are specific to VS Code:
- **Use Agent mode.** Ask and Edit modes don't call MCP tools.
- **Install and log in to the `copilot` CLI.** The steps run headless with it. It must be on the `PATH` VS Code was started with: start VS Code from a shell that has it, or link it into `~/.local/bin`.

**Start a run.** Use any of these:
- **Prompt file:** type `/multiflow bugfix The VM list shows stale IPs after a restart`. The form `/multiflow` with no arguments opens an input box.
- **Custom agent:** pick `multiflow` in the agent dropdown, then describe the task.
- **MCP prompts:** type `/mcp.multiflow.bugfix` or `/mcp.multiflow.run`. VS Code asks for each argument in an input box (`task`, optional `recipe` and `backend`) and completes recipe and backend names.

VS Code asks for confirmation before each tool call. Choose **Allow in this session** (or workspace) for the multiflow tools and for the terminal `sleep` used between polls, or the monitoring loop stops at every poll.

## 4. OpenCode (untested)

Add the server to `opencode.json` in the repository:

```json
{"mcp": {"multiflow": {"type": "local", "command": ["multiflow", "mcp"], "enabled": true}}}
```

Then ask in plain language: "Start a multiflow bugfix run for: the VM list shows stale IPs after a restart, and keep me posted until it finishes." OpenCode has no `/multiflow` command file, so mention multiflow explicitly. Steps run with `opencode run` and model names are `provider/model`. The OpenCode backend has not been tested; if detection picks the wrong harness, ask for `backend: opencode`.

## 5. Plain terminal and CI

No chat is needed: the CLI is the same orchestrator the chats start in the background.

```bash
multiflow list                                         # recipes
multiflow show bugfix                                  # nodes, models, permissions
multiflow run bugfix "The VM list shows stale IPs after a restart" -b claude
multiflow run feature --task-file task.md -m review=gpt-6.1-sol --timeout 1800
```

Outside any harness, detection falls back to `copilot`, so pass `-b` in terminals and CI. `run` exits `0` when the run completes and `1` when it halts or fails, so it can gate a CI job:

```bash
multiflow run quick_patch --task-file task.md -b copilot --timeout 1800 || { multiflow status --json; exit 1; }
```

## 6. Any other MCP client

Start `multiflow -w /path/to/repo mcp` as a stdio server. To run a workflow, the client:
1. calls `multiflow_list_recipes`;
2. calls `multiflow_start(recipe, task)`, adding `backend` and `models` only if needed;
3. polls `multiflow_status(run_id)` until `status` is no longer `running`;
4. reads the results with `multiflow_read_artifact(run_id, name)`.

Parameters and return values: [guide §7](multiflow-guide.md#7-mcp-tool-reference).

## 7. Steering a run from any harness

These requests work in every chat. Each sentence maps to a `multiflow_start` argument:

| You say | The chat passes |
| --- | --- |
| "use the feature recipe" | `recipe: "feature"` |
| "use opus for every step" | `models: {"*": "opus"}` |
| "use gpt-6.1-sol for the review" | `models: {"review": "gpt-6.1-sol"}` (node names: `multiflow show <recipe>`) |
| "run the steps with Claude Code" | `backend: "claude"` (only needed to differ from the current harness) |

Model ids must be valid for the backend that runs the steps ([guide §4](multiflow-guide.md#4-choosing-models)).

Chat-started runs are detached, so closing the chat doesn't stop them. From any terminal:

```bash
multiflow list runs                         # all runs, newest last
multiflow status                            # latest run: steps, verdicts, artifacts
tail -f .multiflow/runs/<id>/runner.log     # live runner output
multiflow resume <id>                       # continue a run shown as "stopped"
multiflow resume --extra-attempt <id>       # one more try after a "halted" run
multiflow resume -b claude <id>             # continue on another harness, e.g. when one is rate-limited
```

If something doesn't work, see [guide §13](multiflow-guide.md#13-troubleshooting-and-limitations). The most common cause is `multiflow` missing from the harness's `PATH`.
