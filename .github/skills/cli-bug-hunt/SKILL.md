---
name: cli-bug-hunt
description: 'Long exploratory CLI testing session against the Multipass snap to find and reproduce bugs in a given area, writing simple repro scripts to a memory folder. Takes two args: the Multipass area to test and the memory folder.'
argument-hint: '<area> <memory-folder>'
---

# Multipass CLI bug hunt

## Arguments

This skill takes two arguments:

1. `<area>`: the particular area of Multipass to explore (e.g. `networking`, `mounts`, `snapshots`, `aliases`). If missing, ask the user.
2. `<memory-folder>`: the folder holding repro scripts of already-found bugs. New bugs are recorded here at the end.
   If it is not given, or you lack permission to read or write it, create a new folder in the current workspace (e.g. `./multipass-bug-hunt/`) and use that instead, telling the user which folder you used.

## Task

The environment has a `multipass` snap installed and possibly the matching codebase. Run a long CLI testing session exploring and breaking edge cases in `<area>`. Do not just do the ordinary: look for unusual inputs, odd command sequences, boundary values, repeated/partial/interrupted operations, bad state transitions, and so on.

The goal is to find ways of breaking Multipass **through the CLI**.

## Plan first

Before running any test, read `<memory-folder>` (see below) and write a plan: the sub-areas, commands and edge-case ideas you will explore, ordered by how likely they are to break things and excluding already-found bugs. Keep it as a checklist, follow it during the session, and update it as you learn.

## Rules

- **Read `<memory-folder>` first.** Scripts there are bugs already found. Do not explore or report them again.
- Only use `multipass` commands, or other tools that assist you (shell utilities, `snap`, `ip`, etc.). Never tamper directly with Multipass's internal files, data directories or daemon state.
- If the codebase is available, use it only as a guide to find suspicious spots, never as the source of truth. Real CLI behavior decides what is a bug.
- Do not exploit concurrency bugs (e.g. running commands with `&`), as concurrency is not currently supported, unless `<area>` explicitly asks for it.
- **Reproduce before reporting.** A bug is only included once you have confirmed it by reproducing it. Discard anything you cannot reproduce.
- Clean up instances and other resources you create when done.

## Reproduction scripts

For every confirmed bug, write a script that runs the same commands and reproduces the issue. Put it in `<memory-folder>` (or the fallback workspace folder) at the end of the session.

- Simple commands only (`multipass launch`, `multipass exec`, ...). **Do not use variables**, loops or helper functions; hard-code names and values.
- Add a small explanation as comments at the top: what the bug is, expected behavior, and actual behavior.
- One script per bug, with a descriptive file name.
- Run each script once more to verify it reproduces the issue.

Example:

```sh
#!/bin/sh
# Bug: `multipass set` accepts an empty value for local.driver and breaks later calls.
# Expected: an error is returned. Actual: the command succeeds and `multipass get` then fails.
multipass set local.driver=
multipass get local.driver
```

## Final report

Briefly list each confirmed bug with its script path, plus areas explored that turned up nothing.
