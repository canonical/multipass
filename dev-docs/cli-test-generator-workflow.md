# How to run the CLI Behavior Test Generator locally

This is the local workflow for turning a single GH issue and/or PR into a
reviewed Gherkin spec and a matching `tests/cli` pytest, using Copilot (CLI
or VS Code Copilot Chat) and the repository's own agent definition.

The goal is to end up with, for one bug/small feature:

1. a Gherkin `.feature` file describing the expected CLI behavior,
2. a pytest file in `tests/cli/` implementing it — indistinguishable in
   location/naming from a hand-written test,
3. a local git commit containing both — left for you to push, amend, or
   discard.

## Files involved

- Agent definition: [.github/agents/cli-test-generator.agent.md](../.github/agents/cli-test-generator.agent.md)
- Context-fetch script: [tools/cli-test-generator/fetch-context.sh](../tools/cli-test-generator/fetch-context.sh)
- Output locations: `tests/cli/features/<slug>.feature` and `tests/cli/cli_<slug>_test.py`
- CLI test suite conventions: [tests/cli/README.md](../tests/cli/README.md)
- Shared `.agent.md` structure this follows: [dev-docs/agent-definition-template.md](agent-definition-template.md)

## Prerequisites

```bash
cd multipass
which gh
which jq
gh auth status
```

If you want step 7 (best-effort local test run) to actually execute the
generated test, also set up the `tests/cli` Python environment per its
[README](../tests/cli/README.md#installing-the-tests) and have a local
Multipass build available.

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
5. Only after your approval does it generate the pytest file from that
   exact Gherkin, then review the mapping between the two before finalizing.
6. It offers to compile-check (and, if you have a local build, run) the
   generated test.
7. It stages only the two new files, shows you `git diff --stat`, and asks
   before creating a **local** commit. It will not push, open a PR, or touch
   any other file.

## End result

After a run, you should have:

- `tests/cli/features/<slug>.feature` — the approved scenarios,
- `tests/cli/cli_<slug>_test.py` — the generated test, sitting right next to
  the hand-written suites, collected by the default `pytest tests/cli` run
  like any other test file — there is no quarantine directory or marker,
- one local commit containing both (nothing else),
- no changes to CI configuration, pytest markers, or any existing test file.

Once you push the commit, this test is just another `tests/cli` test; the
only record of how it was produced is the commit message and the companion
`.feature` file. Review the generated pair like you would any other diff:

```bash
pytest tests/cli/cli_<slug>_test.py --daemon-controller=standalone --bin-dir=build/bin
```

Push the commit, squash it into your feature branch's history, or drop it
entirely — that decision is always yours, never the agent's.

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
