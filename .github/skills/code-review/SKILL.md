---
name: code-review
description: Review Multipass pull requests and code changes. Use when reviewing a PR, a diff, or uncommitted changes in this repository.
---

# Reviewing Multipass code

Apply this workflow to every review. Keep comments few and high-signal: a review that says nothing wrong is better than one that says something unverified.

## 1. Context

Identify which architectural layers the diff touches: proto/RPC, daemon, platform backends, CLI, Flutter GUI, build system, or tests. Load the relevant conventions from `CONTRIBUTING.md` before judging anything.

## 2. Architecture checks

Apply the checks for each touched area:

| Diff touches | Check |
|---|---|
| `src/rpc/multipass.proto` | Backward/forward compatibility of the streaming RPC; no field renumbering or reuse; client impact (CLI and Flutter GUI) considered |
| `src/platform/backends/` | Feature parity or justified asymmetry across backends (qemu, applevz, hyperv, virtualbox); abstraction stays in the common interface — no per-platform leakage |
| `src/daemon/` | VM state machine transitions remain valid; instance metadata/snapshot persistence unchanged or migrated; gRPC reply ordering on streams preserved |
| `src/client/gui/` | Provider/state-management conventions followed; Dart test files keep the `_test.dart` suffix (required by the Flutter test runner) |
| `tests/` | Mocks updated alongside interface changes; error paths covered, not just happy paths |
| `CMakeLists.txt`, `*.cmake` | Modern target-centric idioms; no repeated `find_package`; alphabetical ordering where the file already has it |

## 3. Core rules

1. **Verify before asserting.** Never claim a defect you cannot confirm in the actual code — no asserted compile/link failures, dangling pointers, or API-contract violations without checking the callee's real implementation. If unverifiable, ask a question instead.
2. **Judge the fix's layer, not just its correctness.** Flag changes that patch a call site instead of the root cause (daemon vs CLI, base class vs backend), or that duplicate existing helpers instead of reusing them.
3. **Scrutinize concurrency and lifetimes.** Multipass is heavily multi-threaded (daemon operations, Qt signals, QMP, libssh callbacks). Check lock scoping, condition-variable waits (predicate form), and that callbacks cannot outlive their objects.
4. **Require tests, including error paths, and preserve mockability.** New code needs unit tests under `tests/` per project conventions; new external dependencies must stay behind injectable, mockable interfaces.
5. **Silence is golden on style and scope.** Never comment on formatting (clang-format and dart format own it), Qt/gRPC idioms (signal/slot async, streaming RPCs, QMP over stdio), or code outside the diff. Matching the surrounding code is intentional — say nothing.

## 4. Verification pass

Before posting any comment, re-read the finding and the actual code it refers to. Delete the finding if you cannot point to the specific lines that prove it. Never assert build failures, memory-safety bugs, or third-party API behavior you have not verified in this tree.

## 5. Output

Report only verified findings, ordered by severity (correctness > concurrency > architecture > tests). Phrase each as the concrete problem and a suggested fix. If the change is sound, approve with a short summary rather than inventing nits.
