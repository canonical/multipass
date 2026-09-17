# Multipass — Copilot Instructions

Agent-specific instructions for GitHub Copilot working in this repository.

**General project knowledge — architecture, layout, build, testing, language
conventions, security, and Git policy — lives in [AGENTS.md](../../AGENTS.md) at the
repository root. Read and follow it for all tasks.** This file only covers
Copilot-specific behavior.

## Skills: what to use when

- **Code review** — When reviewing a pull request, a diff, or uncommitted changes,
  apply the workflow and rules in `.github/skills/code-review/SKILL.md`. In short:
  verify every finding against the actual code before asserting it, keep comments
  few and high-signal, and stay silent on formatting and intentional project idioms.

## Tooling notes

- **Formatting** — Run formatters before finishing a task, per AGENTS.md:
  `clang-format -i` for C++ (project `.clang-format`), and the vendored Flutter SDK
  (`3rd-party/flutter/bin/dart format`) for Dart — not whatever is on `PATH`.
- **Validation honesty** — Do not claim a build, test, or lint check passed unless
  it was actually run in this session. Name the checks run in your final summary
  and flag any relevant ones that could not be run.
- **Generated and vendored files** — Never edit `src/client/gui/lib/generated/`,
  anything under `build/`, or the git submodules under `3rd-party/`
  (`flutter`, `jsoncpp`, `protobuf.dart`, `vcpkg`). Regenerate instead.
- **Git operations** — Do not commit, create branches, rebase, or modify submodules
  unless the user explicitly asks. Commit messages must satisfy
  `git-hooks/commit-msg.py` (see AGENTS.md for the format).
