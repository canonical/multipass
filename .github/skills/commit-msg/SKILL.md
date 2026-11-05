---
name: commit-msg
description: Draft a commit message for the currently staged changes that follows the Multipass commit message guidelines (GUIDELINES.md, "Commit messages (MSG)"). Use when asked to write or suggest a commit message. Does not create the commit.
---

Draft a commit message for the currently staged changes. Do not create the commit.

1. Inspect the staged changes with `git diff --cached`. If nothing is staged, say so and stop.
2. Follow the rules in [GUIDELINES.md](../../../GUIDELINES.md#commit-messages-msg) under
   "Commit messages (MSG)", and consider "Git usage (GIT)". If the changes look
   like more than one logical change (GIT1, GIT2, MSG3), point that out and suggest how to
   split them. The `.gitmessage` template in the repository root summarizes the format.
3. For category names and style, look at recent history (`git log --no-merges -n 30`),
   giving more weight to commits by the current user (match `git config user.email` as well
   as `git config user.name`, since authors may appear under several names) and to commits
   touching the same files. If the current user has no commits, rely on the general history.
4. Write the draft to a temporary file, check it with
   `python3 git-hooks/commit-msg.py <file>`, and fix any violations it reports.
5. Output only the final message in a code block, with an optional brief note on the
   category choice or split suggestions.
