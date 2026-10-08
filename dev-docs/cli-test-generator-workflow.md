# How to run the CLI Behavior Test Generator locally

This is the local workflow for turning a single GH issue and/or PR into a
reviewed Gherkin spec and a matching `tests/cli` pytest, using Copilot (CLI
or VS Code Copilot Chat) and the repository's own agent definition.

The goal is to end up with, for one bug/small feature:

1. a Gherkin `.feature` file describing the expected CLI behavior,
2. the matching pytest scenario(s) in `tests/cli/` — added to whichever
   existing file already covers that command/feature area if one fits
   (e.g. `cli_mount_test.py`), or a new, aptly-named file if none does
   (never a file named after the issue/PR number, e.g. no
   `cli_issue_1234_test.py`) — indistinguishable in location/structure
   from a hand-written test, with the full Gherkin copied verbatim into a
   comment block directly above the relevant test class,
3. nothing else — the agent never runs git. Staging, committing, and
   pushing those files is entirely up to you, with your own git tooling
   (this matters because the repo may be checked out in a way the agent
   can't reliably run git against, e.g. a worktree mounted over sshfs).

## Files involved

- Agent definition: [.github/agents/cli-test-generator.agent.md](../.github/agents/cli-test-generator.agent.md)
- Context-fetch script: [tools/cli-test-generator/fetch-context.sh](../tools/cli-test-generator/fetch-context.sh)
- Scripted non-interactive alternative: [tools/cli-test-generator/generate-gherkin.sh](../tools/cli-test-generator/generate-gherkin.sh) and [tools/cli-test-generator/generate-test.sh](../tools/cli-test-generator/generate-test.sh)
- Output locations: `tests/cli/<slug>.feature` (always new) and a pertinent
  `tests/cli/cli_<feature>_test.py` (existing file extended, or a new one
  created — not named after `<slug>`)
- CLI test suite conventions: [tests/cli/README.md](../tests/cli/README.md)
- Shared `.agent.md` structure this follows: [dev-docs/agent-definition-template.md](agent-definition-template.md)

## Prerequisites

```bash
cd multipass
which gh
which jq
gh auth status
```

If you want step 6/7 (compile-check, or an actual local test run) to work,
also set up the `tests/cli` Python environment per its
[README](../tests/cli/README.md#installing-the-tests). The short version:

```bash
python3 -m venv .venv
source ./.venv/bin/activate      # Windows: .\.venv\Scripts\activate
pip install -e ./tests/cli
```

> **If your checkout is mounted over sshfs/NFS/etc. and `python3 -m venv`
> fails or behaves strangely** (e.g. permission errors or stuck broken
> symlinks under `.venv/bin/`), some network filesystems don't support the
> symlinks a venv needs. Create the venv **outside** the mounted checkout
> instead and point an editable install at the mounted `tests/cli` source
> — the editable install itself doesn't need to create symlinks in your
> checkout:
> ```bash
> python3 -m venv ~/.venvs/multipass-cli-tests
> source ~/.venvs/multipass-cli-tests/bin/activate
> pip install -e /path/to/your/checkout/tests/cli
> ```
> Then activate that same venv (`source ~/.venvs/multipass-cli-tests/bin/activate`)
> from your checkout whenever you want to run `pytest` or the scripts below
> — the venv location doesn't need to match where the repo lives.

Running a generated test for real additionally needs a local Multipass
build (`--daemon-controller=standalone --bin-dir=<path-to-build/bin>`).

## Use the agent locally

Ask your agent something like:

> Use the CLI Behavior Test Generator agent. Generate a behavior test for
> issue #1234 in canonical/multipass — it's a bug fix.

or, for a PR directly:

> Use the CLI Behavior Test Generator agent on PR #5099. Don't look at the
> diff — base the test only on the PR description.

(That last sentence is already a hard rule in the agent definition, but it
doesn't hurt to say it — it's the single most important constraint in this
workflow.)

### What happens, step by step

1. The agent asks for anything missing: issue/PR number, repo, and whether
   this is a bug-fix confirmation or a new-feature acceptance test.
2. It runs `tools/cli-test-generator/fetch-context.sh --issue|--pr <N>` and
   reads only the returned title/body/comments/labels — never the diff.
3. It drafts a `.feature` file and **stops**, showing it to you.
4. **You approve it, or ask for changes.** This gate always happens; it is
   not optional and the agent should not proceed past it on its own.
5. Only after your approval does it generate the pytest scenario(s) from
   that exact Gherkin, choosing the destination deliberately: it extends a
   pertinent existing file if one covers the same command/feature area
   (e.g. `cli_mount_test.py`), or creates a new, aptly-named file if none
   does — never a file named after the issue/PR number. It copies the full
   Gherkin into a comment block directly above the relevant test class,
   then reviews the mapping between the two before finalizing.
6. It offers to compile-check (and, if you have a local build, run) the
   generated test.
7. It stops there. It never runs `git add`, `git commit`, or `git push` —
   the new/edited files are simply left in your working tree for you to
   review, stage, and commit yourself, whenever and however you want.

## Scripted alternative (non-interactive)

If you'd rather not drive this through a chat turn, two standalone scripts
wrap the same agent for non-interactive, scripted use. Script 1 calls
`copilot --agent cli-test-generator -p ... --no-ask-user --available-tools view grep glob create`;
script 2 adds `edit` to that list so it can extend an existing file. The
restricted tool list is a hard guardrail either way: the model cannot run
`bash`/`git`/`gh` during either step, so it cannot read a diff even if it
wanted to. Each script does exactly one thing and then stops.

**Script 1 — draft the Gherkin:**

```bash
./tools/cli-test-generator/generate-gherkin.sh --issue 1234 \
  [--repo canonical/multipass] [--prompt "extra context, treated as trusted"]
```

It fetches behavior-only context via `fetch-context.sh`, asks the model to
write `tests/cli/issue_1234.feature` (or `pr_<N>[_issue_<M>]` for a
PR), then reports its path and exits — it does not print the file's
contents. Nothing else happens — no test is generated yet.
**Review/edit the `.feature` file yourself before moving on** (e.g.
`${EDITOR:-vi} tests/cli/issue_1234.feature` or `cat` it if you want it in
your terminal); this script does not ask you to approve inline, it just
stops so you can.

**Script 2 — generate the test from your approved Gherkin:**

```bash
./tools/cli-test-generator/generate-test.sh --feature tests/cli/issue_1234.feature
```

It reads that exact file (only — never re-fetches the issue/PR), then asks
the model to add the scenario(s) to whichever `tests/cli/cli_*_test.py`
file is most pertinent (editing it if it exists, or creating a new,
aptly-named one if it doesn't — e.g. a mount-related issue lands in
`cli_mount_test.py`/`cli_mount_classic_test.py`, not `cli_issue_1234_test.py`),
with the full Gherkin copied into a comment block directly above the
relevant test class. Since the output path isn't known in advance, the
script snapshots `tests/cli/cli_*_test.py` content hashes before and after
the call to detect exactly which single file changed (erroring out if zero
or more than one file was touched — override with `--test-path` if you want
to force a specific destination). It then compile-checks that file with
`python3 -m py_compile`, reports its path, and exits (no file contents
printed to the terminal — open it yourself to review).

Neither script stages, commits, or pushes anything, and neither ever runs
git at all — staging/committing is still a manual step you decide on,
same as the interactive flow. Run
`pytest tests/cli/<the resulting file> --daemon-controller=standalone --bin-dir=build/bin`
yourself once you're happy with the result.

Use this path when you want full control over each stage from the shell
(e.g. piping into your own review tooling) instead of a chat conversation;
use the interactive agent when you want the hard review gate handled inline
for you — either way, git stays entirely in your hands.

## End result

After a run, you should have:

- `tests/cli/<slug>.feature` — the approved scenarios, always a new file
  named after the issue/PR,
- the matching pytest scenario(s) added to whichever existing
  `tests/cli/cli_*_test.py` file already covers that command/feature area,
  or a new, aptly-named file if none did — sitting right next to the
  hand-written suites either way, collected by the default
  `pytest tests/cli` run like any other test file, with no quarantine
  directory or marker. A `# Gherkin: ...` comment block directly above the
  relevant test class contains the exact Gherkin those tests were generated
  from, so the pairing survives even if the `.feature` file is later moved
  or deleted.
- no git staging, commits, or changes to CI configuration, pytest markers,
  or any existing test beyond that one file's new addition — the agent
  never invokes git, since the repo may be checked out in a way it can't
  run git against reliably (e.g. a worktree mounted over sshfs).

Review the result like you would any other new/changed file:

```bash
pytest tests/cli/<the resulting file> --daemon-controller=standalone --bin-dir=build/bin
```

Stage, commit, amend, squash, or discard the changes with your own git
tooling — that decision is always yours, never the agent's.

## Future work (explicitly not implemented yet)

This phase is deliberately local-only, and generated tests are deliberately
*not* segregated from hand-written ones once committed. Not yet decided or
built:

- Any mechanism (pytest marker or otherwise) to treat these differently in
  CI — e.g. running them only for RCs/releases, or making their failures
  non-blocking elsewhere.
- Any GitHub Action hook (e.g. triggered after first PR approval, or via a
  "post-PR PR" similar in spirit to the release-notes issue-driven trigger)
  that would run this agent automatically instead of locally.

Treat both as separate follow-up proposals once this local workflow has
proven useful on a handful of real issues/PRs.
