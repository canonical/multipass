# How to run the Release Notes Generator locally in VS Code

This is the local workflow for running the Multipass release-notes agent from
inside VS Code, using Copilot Chat and the repo's own agent definition.

The goal is to produce release notes for a specific release by:

1. setting the previous tag and target version,
2. letting Copilot use the local agent definition,
3. generating the enriched commit data,
4. reviewing the ranked PRs and choosing the notes to keep,
5. writing the release notes file and updating the index,
6. opening a draft PR when ready.

## Files involved

- Agent definition: [.github/agents/release-notes-generator.agent.md](../.github/agents/release-notes-generator.agent.md)
- Release-notes issue template: [.github/ISSUE_TEMPLATE/release-notes.md](../.github/ISSUE_TEMPLATE/release-notes.md)
- Commit data script: [tools/release-notes/get-commits-since-release.sh](../tools/release-notes/get-commits-since-release.sh)
- Notes template: [docs/reference/release-notes/RELEASE_NOTES_TEMPLATE.md](../docs/reference/release-notes/RELEASE_NOTES_TEMPLATE.md)
- Index: [docs/reference/release-notes/index.md](../docs/reference/release-notes/index.md)

## Prerequisites

Before you start, make sure you are in the Multipass repo and have the normal
release-notes tooling available:

```bash
cd /Users/scott/Documents/dev/multipass
which gh
which jq
gh auth status
git fetch --tags
```

If you are working from a fork, set:

```bash
export PR_REPO=canonical/multipass
```

If the repo is shallow, fetch more history first:

```bash
git fetch --tags --unshallow 2>/dev/null || git fetch --tags
```

## Use the agent locally

Ask your agent something like:

> Use the Release Notes Generator agent. Generate release notes for PREVIOUS_TAG=v1.16.3 and TARGET_VERSION=1.17.0 in this repo.

If desired, open a draft PR against `main` with the notes file and index update.

## Generating notes for a past release

The same workflow also works for historical regeneration. The only difference is
that you select the correct release range from the repo history.

For example, if you want to regenerate notes for the 1.15 → 1.16 range:

> Use the Release Notes Generator agent. Generate release notes for v1.14.0 since v1.13.1

A few historical ranges are especially noisy because of merged repo history or
cherry-picked backports, so it is worth checking the earlier notes and validation
queries before finalizing the write-up.

## End result

This workflow should leave you with:

- a generated release-notes markdown file under [docs/reference/release-notes](../docs/reference/release-notes),
- an updated index at [docs/reference/release-notes/index.md](../docs/reference/release-notes/index.md),
- and a draft PR that is ready for human review.
