# CLI Behavior Test Generator — quick guide

Turns a single GH issue and/or PR into a reviewed Gherkin spec, then into a matching `tests/cli` pytest. All instructions and prompts live in the agent definition, [.github/agents/cli-test-generator.agent.md](../.github/agents/cli-test-generator.agent.md); this page only shows how to invoke it. The agent never runs git — staging and committing are up to you.

## Prerequisites

- `gh` (authenticated: `gh auth status`) and `jq` in your `PATH`, used by [tools/cli-test-generator/fetch-context.sh](../tools/cli-test-generator/fetch-context.sh).
- To compile/collect-check generated tests, set up the `tests/cli` Python environment per its [README](../tests/cli/README.md#installing-the-tests).
- To run generated tests for real, a local Multipass build (`--daemon-controller=standalone --bin-dir=<path-to-build/bin>`).

## Select the agent

**Copilot CLI** (run from the repository root)

- Start with the agent: `copilot --agent cli-test-generator`
- Or, in a running session, use the `/agent` command and pick
  **CLI Behavior Test Generator**.

**VS Code Copilot Chat**

Open the Chat view, open the agent picker (the mode dropdown next to the input box) and select **CLI Behavior Test Generator**. It is listed because the definition is in `.github/agents/`.

Then just write your request in the chat, as below. Every request is a plain prompt; no flags or scripts are needed.

## VS Code integration: prompt files

Two prompt files in `.github/prompts/` give one-step entry points in VS Code Copilot Chat. Type the slash command in the chat; it selects the agent for you and asks for its inputs:

| Slash command | Mode | Asks for |
|---|---|---|
| `/cli-test-gherkin` | Mode 1 — Generate Gherkin | issue/PR (optionally `in owner/repo`), extra guidance |
| `/cli-test-from-gherkin` | Mode 2 — Generate test from Gherkin | `.feature` path, extra guidance or forced destination file |

The agent's tools (`read`, `search`, `edit`) come from its own definition and include no command execution, so it cannot run git or read a diff. Because it cannot run commands, give it the issue/PR context yourself (run `fetch-context.sh` and paste the JSON, or use `generate-gherkin.sh --interactive`), and after Mode 2 run the checks with `./tools/cli-test-generator/generate-test.sh --check-only --test-path <test file>`; it compiles and collects the test and prints the validation command (in this mode it cannot tell which tests are new, so it selects every class in the file; narrow it to the new tests before running).

## Mode 1 — Generate Gherkin

Prompt examples (typed into the agent in Copilot CLI or VS Code chat):

> Generate Gherkin for issue #1234.

> Generate Gherkin for PR #5099 in canonical/multipass. Focus on the `--all`
> flag.

The agent fetches the issue/PR text (never the diff), asks you clarifying questions, writes `tests/cli/<slug>.feature` (`issue_<N>`, `pr_<N>` or `pr_<N>_issue_<M>`; ask for another name if you want one), shows it, and waits. Review it, ask for changes, or edit the file yourself, and approve it.

## Mode 2 — Generate test from Gherkin

Prompt examples:

> Generate the test from `tests/cli/issue_1234.feature`.

> Generate the test from `tests/cli/issue_1234.feature` and put it in
> `tests/cli/cli_mount_test.py`.

The agent adds one pytest method per scenario to the most pertinent existing `tests/cli/cli_*_test.py` file (or a new aptly-named one), with the Gherkin copied above the test class, then tells you how to run the compile and collection checks; the shell wrapper runs those checks automatically.

## Run it from the shell (scripts)

Two thin wrappers launch `copilot --agent cli-test-generator` for you with a short request; all instructions stay in the agent file. They restrict the model's tools (no `bash`/`git`/`gh`), so it cannot read a diff.

```bash
# Mode 1: writes tests/cli/<slug>.feature, then stops
./tools/cli-test-generator/generate-gherkin.sh --issue 1234 [--repo owner/repo] \
    [--prompt "extra guidance"] [--slug name] [--interactive]

# Mode 2: adds the pytest to the most pertinent tests/cli/cli_*_test.py
./tools/cli-test-generator/generate-test.sh --feature tests/cli/issue_1234.feature \
    [--prompt "extra guidance"] [--test-path tests/cli/cli_mount_test.py]
```

- `--interactive` opens a live session so the agent can ask you questions; without it the run is headless and ambiguities become `# TODO(clarify)` comments.
- `generate-test.sh` detects which file changed and runs the compile and collection checks. Review the `.feature` file before running it.

## Validate red-before-green yourself

The agent never runs the test against a real daemon. Run it against a build of the **unfixed** code:

```bash
pytest tests/cli/<file>::<AddedTestClass> --daemon-controller=standalone --bin-dir=<path-to-build/bin> -rA -v
```

- Only the added tests are selected (`file::Class`, or `-k` for appended
  methods), not the whole file. The agent and `generate-test.sh` fill in the
  real class/method names for you.
- Judge every scenario individually: scenarios for new behavior must FAIL,
  control cases (unchanged behavior) may pass.
- A bare `pytest` without those two flags tests the system snap, not your
  build.
- If a scenario passes when it shouldn't, see the "red-before-green" steps in the agent file.
