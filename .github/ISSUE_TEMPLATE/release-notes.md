---
name: Generate release notes
about: Kick off the Release Notes Generator agent for an upcoming release
title: '[release-notes] Generate notes for X.Y.Z'
labels: documentation
assignees: ''

---

<!--
Fill in the two values below, then assign this issue to @copilot.

The Release Notes Generator agent
(.github/agents/release-notes-generator.agent.md) will:
  1. run ./tools/release-notes/get-commits-since-release.sh --json --enrich --tag <PREVIOUS_TAG>
  2. rank PRs with the signal-net + anchored, batched rubric
  3. write docs/reference/release-notes/<TARGET_VERSION>.md from
     docs/reference/release-notes/RELEASE_NOTES_TEMPLATE.md and update docs/reference/release-notes/index.md
  4. open a draft PR linked back to this issue
-->

**PREVIOUS_TAG:** <!-- last released tag to diff against, e.g. v1.16.3 -->

**TARGET_VERSION:** <!-- version being released, e.g. 1.17.0 -->

## Task

Follow the Release Notes Generator agent
(`.github/agents/release-notes-generator.agent.md`) end to end for the release
above. Use `--enrich` for the commit data, respect the `skip` pre-filter, and
rank PRs with the batched, anchored rubric (do not judge importance from commit
subjects alone). Open the result as a **draft** PR against `main`.
