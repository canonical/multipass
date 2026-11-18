# Tutorial: adding roles and workflows

This tutorial builds a new workflow, `secure_patch`, end to end:
- a new role, `security_reviewer`;
- built-in roles reused with different instructions;
- the recipe that wires them together;
- validation, a dry run and an automated test of the loop;
- a real run.

Every field is listed in [guide §9](multiflow-guide.md#9-writing-roles-and-recipes). This page explains how to put the pieces together and why.

```
implement (implementer) ──done──▶ review (reviewer) ──approved──▶ security (security_reviewer) ──secure──▶ complete
      ▲                                  │                                  │
      └────────── changes_requested ─────┘◀──────────── issues_found ───────┘
```

## 1. Where files go

| What | Project file (this repository only) | Built-in (every repository) |
| --- | --- | --- |
| Role | `<repo>/.multiflow/roles/<name>.md` | `tools/multiflow/multiflow/library/roles/<name>.md` |
| Recipe (workflow) | `<repo>/.multiflow/recipes/<name>.yaml` | `tools/multiflow/multiflow/library/recipes/<name>.yaml` |

Start with project files: they need no reinstall and can be committed with the repository that uses them. A project file with the same name as a built-in **replaces** the built-in for that repository, including inside built-in recipes. Move a role or recipe to the library once it is useful everywhere. With an editable install it is available immediately, and `tools/multiflow/tests/test_multiflow.py` should cover it.

## 2. Decide what the role is for

A role is a persona that several workflows can reuse. Before writing one, answer four questions:
- **What does it own?** A reviewer owns a report, an implementer owns source changes. That decides the tool preset: `read-only` roles can't touch the source tree and write their outputs through the `multiflow_write_output` tool; `edit` roles can change the source but never locked artifacts.
- **What outcomes can it report?** These are its **verdicts**. They are the only thing the state machine looks at, so make them unambiguous decisions, e.g. `approved` / `changes_requested`, never a free-text status. A role that just produces something uses the default `[done]`.
- **Which model fits?** Pick a strong model for judgment-heavy roles and a fast one for mechanical work. Give a per-backend map so the role works in every harness. For reviewers, a different model family than the implementer gives a more independent opinion.
- **Is it really new?** If it differs from an existing role only in focus ("review only the tests"), reuse that role with per-node `instructions` in the recipe instead.

A security reviewer is genuinely new: different expertise, its own verdicts, read-only.

## 3. Write the role

Create `.multiflow/roles/security_reviewer.md`:

```markdown
---
description: Read-only security review of the change.
model: {copilot: gpt-6.1-sol, claude: opus}
tools: read-only
verdicts: [secure, issues_found]
---
You are an application security engineer acting as the **security reviewer**. You did not write this change; judge it independently and only from a security point of view.

Do:
- Inspect the actual change (`git diff`, `git status`, the touched files) rather than trusting the implementation notes.
- Look for injection, path traversal, unsafe deserialization, secrets in code or logs, missing input validation, overly broad permissions and unsafe defaults.
- Write the report to your output: first line `VERDICT: secure` or `VERDICT: issues_found`, then a numbered list of findings with file:line, impact and the required fix.

Don't:
- Modify source files.
- Report style or functional issues; another reviewer covers those.

Return `secure` only if there are no findings that must be fixed.
```

Guidelines for the body:
- **Describe the persona and what a good result looks like.** Don't repeat what multiflow adds to every prompt: the task, input and output artifacts with their paths, the read-only artifacts, the history of earlier steps, feedback from a rejected attempt and how to submit. Look at a step's `prompt.md` to see the full prompt.
- **Refer to artifacts by purpose** ("your output", "the plan"), not by path. The recipe decides the paths, so the role stays reusable.
- **State when each verdict applies.** The next step depends on it, so vague criteria make the workflow loop or finish too early.
- **Use "Do / Don't" lists for boundaries.** File permissions are enforced anyway, but saying so avoids wasted attempts.
- **Add hard bans with `deny_tools`.** Commands the role must never run are blocked per backend, e.g. `deny_tools: {copilot: ["shell(git push)"], claude: ["Bash(git push:*)"]}`, as the built-in `refactorer` does.

`name:` is optional and defaults to the file name. Check that multiflow found the role:

```console
$ multiflow list roles
...
security_reviewer Read-only security review of the change.  [/home/me/src/my-repo/.multiflow/roles/security_reviewer.md]
```

## 4. Design the workflow

A recipe is a state machine. Sketch it before writing YAML:
- **Nodes** are steps. Each runs one role, and the same role can appear in several nodes (e.g. a reviewer for the plan and again for the code).
- **Artifacts** are the files steps hand to each other. A node's `outputs` are the only artifacts it may write. **Every other artifact is locked for it**, so the implementer can't edit the plan or pre-write its own review. Inputs are read-only; mark an input `name?` when it doesn't exist yet on the first pass, like a review that only exists after the first iteration.
- **Transitions** map each verdict of the node's role to the next node, `__complete__` or `__fail__`. Every verdict the role declares must be routed, and only declared verdicts may be routed.
- **Loops** go back to an earlier node, carrying feedback as an input artifact. `max_iterations` bounds how often each node may run, so a loop can't go on forever. Use `__fail__` only for an explicit "this cannot be done" outcome.

For `secure_patch`:

| Node | Role | Reads | Writes | Verdict → next |
| --- | --- | --- | --- | --- |
| `implement` | implementer | `review?`, `security?` | `notes` | `done` → `review` |
| `review` | reviewer | `notes` | `review` | `approved` → `security`, `changes_requested` → `implement` |
| `security` | security_reviewer | `notes` | `security` | `secure` → complete, `issues_found` → `implement` |

## 5. Write the recipe

Create `.multiflow/recipes/secure_patch.yaml`:

```yaml
description: Implement a small change, then a functional review and a security review; any finding loops back to the implementer.
max_iterations: 3

artifacts:
  notes:    {path: implementation_notes.md, description: what changed and how it was validated}
  review:   {path: review.md, description: functional review of the latest iteration}
  security: {path: security.md, description: security review of the latest iteration}

nodes:
  implement:
    role: implementer
    inputs: ["review?", "security?"]
    outputs: [notes]
  review:
    role: reviewer
    inputs: [notes]
    outputs: [review]
    instructions: Ignore security aspects; they are reviewed in a separate step.
  security:
    role: security_reviewer
    inputs: [notes]
    outputs: [security]

entry: implement
transitions:
  implement: {done: review}
  review:    {approved: security, changes_requested: implement}
  security:  {secure: __complete__, issues_found: implement}
```

Points to notice:
- **The `description` matters.** Chats read it to choose a recipe when the user doesn't name one, and it is part of every step's prompt. State the goal and any invariant every step must respect; the built-in `pr_split` puts its "combined diff equals the original" rule there.
- **`instructions` specializes a reused role** for one node without copying it: here the generic reviewer is told to leave security to the next step.
- **`"review?"` is quoted**, because `?` is special in YAML flow lists. Also quote descriptions containing `, ` inside `{...}`, or use block style.
- **Reviews are overwritten on each iteration**, which suits feedback loops. Use separate artifacts when a later step needs every version.
- **Per-node settings override the role.** `model:` sets the node's model and `tools:` its tool preset; e.g. give the first `implement` visit a cheaper model through a separate node, or make a read-only "plan the fix" node from the implementer role.

## 6. Validate and inspect

```console
$ multiflow validate secure_patch
ok      secure_patch
$ multiflow show secure_patch
secure_patch: Implement a small change, then a functional review and a security review; any finding loops back to the implementer.
max_iterations=3 max_step_retries=1  entry=implement

  [implement] role=implementer tools=edit model={'copilot': 'claude-sonnet-5.5', 'claude': 'sonnet'}
     in:  review?, security?
     out: notes
     read-only: review, security
     --done--> review

  [review] role=reviewer tools=read-only model={'copilot': 'gpt-6.1-sol', 'claude': 'opus'}
     in:  notes
     out: review
     read-only: notes, security
     --approved--> security
     --changes_requested--> implement

  [security] role=security_reviewer tools=read-only model={'copilot': 'gpt-6.1-sol', 'claude': 'opus'}
     in:  notes
     out: security
     read-only: notes, review
     --secure--> DONE
     --issues_found--> implement
```

Check the `read-only:` lines: they are the permission model, and they should match your intent. `validate` explains mistakes precisely, e.g. a misspelled verdict or an artifact that isn't declared:

```console
INVALID secure_patch: recipe 'secure_patch': node 'security': role 'security_reviewer' verdicts ['issues_found'] have no transition; node 'security': transitions ['isues_found'] are not verdicts of role 'security_reviewer'
INVALID secure_patch: ... node 'security': unknown input artifact 'plan'
```

## 7. Dry run

The `fake` backend runs the whole machine in-process without an LLM: it writes placeholder outputs and submits each role's **first** verdict, so it follows the "happy path".

```console
$ multiflow run secure_patch "Add input validation to the upload handler" -b fake
run 20261116-184305-secure_patch-c83b -> .multiflow/runs/20261116-184305-secure_patch-c83b
[1] implement (role=implementer, model=default, tools=edit) ...
    -> done: dry run of implement
[2] review (role=reviewer, model=default, tools=read-only) ...
    -> approved: dry run of review
[3] security (role=security_reviewer, model=default, tools=read-only) ...
    -> secure: dry run of security
run 20261116-184305-secure_patch-c83b: COMPLETED (security -> secure)
```

Read `.multiflow/runs/<id>/steps/03-security/prompt.md` to see exactly what the security reviewer will be told: your role body, the node instructions, its inputs and outputs, the locked artifacts and the submit contract. Order the verdicts in the role so the first one is the happy path.

## 8. Test the loops

To exercise the other branches, script the agent with `FakeBackend`. Each call plays one step: it writes the step's outputs and submits a chosen verdict. Save this as `tests/test_secure_patch.py` in the repository and run it with `python -m pytest`, using the Python environment multiflow is installed in:

```python
from pathlib import Path

from multiflow import runner
from multiflow.backends.fake import FakeBackend
from multiflow.step import write_result


def agent_from(verdicts):
    """Fake agent: writes every output and submits the next scripted verdict for the node."""
    def agent(inv, ctx):
        verdict = verdicts[ctx.node].pop(0)
        for path in ctx.outputs.values():
            Path(path).write_text(f"{ctx.node}: {verdict}\n")
        write_result(Path(inv.step_dir), verdict, f"{ctx.node} said {verdict}")
        return 0
    return agent


def test_security_findings_loop_back_to_implementer():
    workspace = Path(__file__).resolve().parents[1]
    run_dir = runner.create_run(workspace, "secure_patch", "the task", "fake")
    state = runner.execute(run_dir, FakeBackend(agent_from({
        "implement": ["done", "done"],
        "review": ["approved", "approved"],
        "security": ["issues_found", "secure"],
    })))
    assert state.status == "completed"
    assert [h.node for h in state.history] == ["implement", "review", "security", "implement", "review", "security"]
```

Give more `issues_found` verdicts than `max_iterations` allows and the run ends `halted`, which shows the loop limit works. This test writes runs into the repository's `.multiflow/runs/`; pass a temporary copy of the workspace if that matters. The built-in recipes are tested the same way in `tools/multiflow/tests/test_multiflow.py`.

## 9. Run it for real

```bash
multiflow run secure_patch "Add input validation to the upload handler"
multiflow run secure_patch --task-file task.md -m security=claude-opus-5.5     # model for one node
```

From a chat, name it like any built-in: `/multiflow secure_patch Add input validation to the upload handler`. Starting a new recipe by name works immediately. The MCP prompts (`/mcp.multiflow.secure_patch` in VS Code) and the recipe list in `multiflow_start`'s schema are built when the server starts, so restart the MCP server to see the recipe there.

After the first real runs, read each step's `prompt.md` and `log.txt` and tune the role body. The most common fixes are sharper verdict criteria (when reviews loop until the limit) and an explicit "write findings only to your output" (when a step is rejected for touching locked artifacts).

## 10. Variations

- **Override a built-in role for one repository.** Copy it (`multiflow list roles` shows the source path) to `.multiflow/roles/<same name>.md` and edit it. Every recipe in that repository, built-in ones included, picks up your version.
- **Several nodes, one role.** `pr_split` uses the `reviewer` role twice, for `plan_review` and `verify`, with different inputs, outputs and instructions.
- **Plan reviews.** Put a reviewer node between the planner and the implementer, and route `changes_requested` back to the planner. The plan is locked for every node except the planner.
- **Explicit failure.** Give a role a verdict like `infeasible` and route it to `__fail__`, so the run ends `failed` (reason `<node> -> infeasible`; the step's summary in the history says why) instead of looping.
- **Different limits.** Raise `max_iterations` for long review cycles, use `max_visits` on a single node, or set `max_step_retries: 0` to halt on the first rejected attempt.
- **Share it.** Move the files into `tools/multiflow/multiflow/library/`, add a test like the one in step 8 to `tools/multiflow/tests/test_multiflow.py`, and list the recipe in the integration files' argument hints and recipe lists (`.claude/commands/multiflow.md`, `.github/prompts/multiflow.prompt.md`, `.github/agents/multiflow.agent.md`) so chats know when to choose it.

Only `kind: role` nodes exist today. Sub-recipes and parallel fan-out are reserved for future versions ([guide §13](multiflow-guide.md#13-troubleshooting-and-limitations)).
