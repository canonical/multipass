# Multipass — Copilot Instructions

Agent-specific instructions for GitHub Copilot working in this repository.

**General project knowledge — architecture, layout, build, testing, language
conventions, security, and Git policy — lives in [AGENTS.md](../AGENTS.md) at the
repository root. Read and follow it for all tasks.** This file only covers
Copilot-specific behavior.

## Skills: what to use when

- **Code review** — When reviewing a pull request, a diff, or uncommitted changes,
  apply the workflow and rules in `.github/skills/code-review/SKILL.md`. In short:
  verify every finding against the actual code before asserting it, keep comments
  few and high-signal, and stay silent on formatting and intentional project idioms.

## Custom agents

- **`master-code-reviewer`** (`.github/agents/`): in-depth implementation review of a working
  tree, commit, or branch, built on the code-review skill. Does not edit files.
- **`master-software-architect`** (`.github/agents/`): system-level design, architecture review,
  and tradeoff advice. Read-only.
