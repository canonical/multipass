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
  1. run ./tools/release-notes/get-commits-since-release.sh --json --tag <PREVIOUS_TAG> --to <TARGET_TAG>
  2. rank PRs with the signal-net + anchored, batched rubric
  3. write docs/reference/release-notes/<VERSION>.md (bare version, no "v") from
     docs/reference/release-notes/RELEASE_NOTES_TEMPLATE.md and update docs/reference/release-notes/index.md
  4. open a draft PR linked back to this issue
-->

**PREVIOUS_TAG:** <!-- add last released tag to diff against, e.g. **PREVIOUS_TAG:v1.16.3** -->

**TARGET_TAG:** <!-- add tag being released, e.g. **TARGET_TAG:v1.17.0** -->

## Task

Follow the Release Notes Generator agent
(`.github/agents/release-notes-generator.agent.md`) end to end for the release
above. Respect the `skip` pre-filter, and rank PRs with the batched, anchored
rubric (do not judge importance from commit subjects alone). Open the result as
a **draft** PR against `main`.

## If generation fails

If enrichment reports it failed for all/most PRs while `gh auth`, `PR_REPO`, and
the tags are all fine, it is almost certainly GitHub's secondary rate limit. The
script batches its GitHub requests and caches only successful fetches, so simply
**re-run** to retry the misses (delete `${TMPDIR:-/tmp}/mp-pr-cache` first only
if you want a guaranteed clean slate). For an unexpectedly broad diff, confirm
`PREVIOUS_TAG` is an ancestor of `TARGET_TAG`
(`git merge-base --is-ancestor "$PREVIOUS_TAG" "$TARGET_TAG"`) before proceeding.

Contributor detection is release-aware and stateless: a contributor is listed
as new only when none of their PRs shipped in a published release before the
target, so a contributor first shipped in a maintenance release is not
acknowledged again by a later feature release. No manual step is required.
