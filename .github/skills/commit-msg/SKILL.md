---
name: commit-msg
description: Draft a commit message for the staged changes following the Multipass guidelines. Use when asked to write or suggest a commit message. Does not create the commit.
---

Inspect the staged changes with `git diff --cached` and draft a commit message following
"Commit messages (MSG)" in [GUIDELINES.md](../../../GUIDELINES.md#commit-messages-msg). Do not
create the commit. Pick a category consistent with `git log` for the touched files, and point
out if the changes should be split into separate commits.

Check the draft with `python3 git-hooks/commit-msg.py <file>` and fix any violations it reports.
