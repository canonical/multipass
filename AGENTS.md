# Multipass — Agent Guide

General project knowledge for AI coding agents and contributors. Tool-specific
instructions (e.g. GitHub Copilot behavior) live in `.github/copilot-instructions.md`.

## Project and Layout

Multipass is a cross-platform VM manager with a **client-daemon architecture**
communicating via **gRPC streaming**. Three user-facing components:

- `multipassd` — privileged C++ daemon that owns VM state and exposes the gRPC API
  (`src/daemon/`).
- `multipass` — C++ command-line client (`src/client/cli/`, subcommands in
  `src/client/cli/cmd/*.cpp`, one file per command; shared client code in
  `src/client/common/`).
- `multipass.gui` — Flutter/Dart desktop client (`src/client/gui/`) with a Dart gRPC
  client, Provider state management, and FFI for native integration.

Other key directories:

- `src/rpc/multipass.proto` — the protobuf/gRPC contract shared by daemon and clients.
  All RPCs use bidirectional `ServerReaderWriter` streams, not simple request-response.
- `include/multipass/` — public C++ interfaces; implementations live under `src/`.
- `src/platform/backends/` — hypervisor backends implementing the common
  `VirtualMachine` / `VirtualMachineFactory` interfaces:
  - `qemu/` — primary backend, controlled via QMP (JSON over the process stdio)
  - `applevz/` — macOS Virtualization.framework
  - `virtualbox/` — VBoxManage integration (macOS/Windows)
  - `hyperv/` — Windows Hyper-V via PowerShell cmdlets
  - `shared/` — common base classes (`BaseVirtualMachine`, `BaseSnapshot`)
- `src/image_host/`, `src/daemon/default_vm_image_vault.cpp` — image hosts (Ubuntu
  SimpleStreams, custom URLs) and the image vault (fetch/verify/cache).
- `src/utils/`, `src/network/`, `src/ssh/`, `src/sshfs_mount/`, `src/process/` —
  cross-cutting concerns.
- `tests/unit/` — mostly flat GoogleTest/GoogleMock C++ suite
  (`test_<area>.cpp`, not a mirror of the source tree) with extensive mock
  infrastructure (nearly every interface has a `mock_*.h`).
- `tests/cli/` — pytest end-to-end tests driving installed or source-built binaries.
- `rxx/` — Rust workspace (`namegen` and `macros` crates) with C++ bridge/CMake
  integration; Rust 1.91, edition 2024.
- `3rd-party/flutter`, `3rd-party/jsoncpp`, `3rd-party/protobuf.dart`,
  `3rd-party/vcpkg` — **git submodules: do not edit them for Multipass changes.**
  The overlay ports in `3rd-party/vcpkg-ports/` and triplets in
  `3rd-party/vcpkg-triplets/` are owned by this repository and may be changed when
  dependency work requires it.

### Key interfaces

- `VirtualMachine` — core VM abstraction all backends implement
  (`include/multipass/virtual_machine.h`)
- `VirtualMachineFactory` — VM creation; `fetch_type()` declares image requirements
- `VMStatusMonitor` — VM state change notifications; states follow a strict machine
  (`off, starting, restarting, running, delayed_shutdown, suspending, suspended,
  unknown`) persisted as JSON by the daemon
- `VMImageVault`, `SSHKeyProvider`, `MountHandler`, `Snapshot` (hierarchical
  parent-child, stored as QJsonObject)

### Architectural patterns

- **gRPC streaming** — daemon handlers in `src/daemon/daemon_rpc.cpp` (routing) and
  `src/daemon/daemon.cpp` (business logic); handle both directions and disconnections.
- **Qt signals/slots** — the primary async pattern; avoid raw threading and respect
  thread affinity.
- **Cloud-init** — VMs are provisioned via a per-instance cloud-init ISO
  (meta-data, user-data, vendor-data, network-config) generated at launch.
- **Errors** — use the specific exception hierarchy
  (`include/multipass/exceptions/...`), not generic errors.
- **Logging** — `multipass::logging` (`mpl::info(vm_name, ...)`).

## Setup and Build

- Top-level `CMakeLists.txt` is authoritative: CMake 3.29, C++20, Qt6, Cargo.
- Fresh setup: follow `BUILD.linux.md`, `BUILD.macOS.md`, or `BUILD.windows.md`.
- Use an existing configured `build/` for incremental work; reconfigure only when
  blocked or when changing build options.
- Tests exist only when configured with `MULTIPASS_ENABLE_TESTS=ON`; the GUI is
  controlled by `MULTIPASS_ENABLE_FLUTTER_GUI`.

Run commands from the repository root. Prefer narrow targets while iterating:

```bash
cmake --build build --parallel
cmake --build build --parallel --target multipassd
cmake --build build --parallel --target multipass
cmake --build build --parallel --target multipass_cpp_tests
```

## Testing

Build `multipass_cpp_tests` before running a focused C++ test. CTest registers the
C++ suite, the real-SSH integration suite, and the Rust tests:

```bash
./build/bin/multipass_cpp_tests --gtest_filter='SomeSuite.SomeTest'
ctest --test-dir build --output-on-failure -R '^multipass_cpp_tests$'
ctest --test-dir build --output-on-failure -R '^multipass_rust_tests$'
ctest --test-dir build --output-on-failure
# Run directly when diagnosing the real-SSH integration executable:
./build/bin/multipass_integration_tests
```

CLI tests require privilege escalation. Standalone mode needs the directory
containing both source-built binaries:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -e ./tests/cli
pytest tests/cli --daemon-controller=standalone --bin-dir=build/bin
# Narrow by marker, file, or keyword:
pytest tests/cli -m snapshot --daemon-controller=standalone --bin-dir=build/bin
pytest tests/cli -k shell_test --daemon-controller=standalone --bin-dir=build/bin
```

See `tests/cli/README.md` for other daemon controllers, storage options, and
diagnostics.

## Language Workflows

Follow `CONTRIBUTING.md`. Its most important C++ rules:

- Standard C++20 and `#pragma once`.
- `CamelCase` for types, `snake_case` for functions and variables.
- Fully initialize objects in constructors; non-copyable/movable types inherit
  `DisabledCopyMove`.
- Keep platform-dependent behavior in dedicated platform units; do not scatter
  platform `#ifdef`s.
- Wrap mockable free functions and external APIs with `MockableSingleton`.
- Avoid const by-value parameters, magic numbers, duplicated sources of truth, and
  warnings; non-MSVC builds use `-Werror`.
- C++ formatting follows `.clang-format` (LLVM base, 4-space indent, 100 columns,
  left pointer alignment, braces on new lines). Run `clang-format -i` on changed
  files before committing.

For GUI work, use the **vendored SDK**, not whatever is on `PATH`:

```bash
cd src/client/gui
../../../3rd-party/flutter/bin/dart format --output=none --set-exit-if-changed .
../../../3rd-party/flutter/bin/flutter analyze
../../../3rd-party/flutter/bin/flutter test
```

Do not hand-edit `src/client/gui/lib/generated/` — changes to
`src/rpc/multipass.proto` regenerate these Dart bindings during the GUI build.

Rust tools run from `rxx/`:

```bash
cargo fmt --check
cargo clippy --workspace --all-targets -- -D warnings
cargo test --workspace
```

## Implementation Rules

- The daemon is the source of truth for VM state. CLI/GUI behavior needing daemon
  state must go through the gRPC contract — never introduce a second state store.
- Changes to `src/rpc/multipass.proto` affect daemon, CLI, GUI, and generated
  bindings; update all consumers and tests together. Treat it as a stable wire
  contract: do not renumber or reuse field numbers; prefer adding new optional
  fields/messages over changing existing ones.
- Platform behavior belongs behind interfaces in `include/multipass/` with
  implementations under `src/platform/` or dedicated platform units. New features
  must work across all supported backends — avoid direct platform APIs.
- Feature flags are declared in `feature-flags.cmake`; follow `CONTRIBUTING.md`
  `FF1`–`FF9` — disabling a flag must preserve pre-feature behavior.
- Most VM operations are async with state callbacks — do not assume synchronous
  behavior.
- Keep changes small and task-focused. Do not refactor unrelated code, edit
  generated/build output, or fix unrelated failures.
- Add or update tests beside changed behavior.
- Keep user-facing documentation in `docs/` (Sphinx: tutorial, how-to-guides,
  reference, explanation) in sync with behavior changes — new/changed commands,
  settings, CLI flags, and workflows need corresponding doc updates. Internal
  design notes live in `dev-docs/`.
- Dependencies follow `CONTRIBUTING.md` `DEP1`–`DEP2`: do not vendor copied source;
  new C++ dependencies are vcpkg ports (repo-owned overlays/triplets under
  `3rd-party/vcpkg-ports/` and `3rd-party/vcpkg-triplets/`).
- Settings are persistent and validated; use the settings handlers
  (`MP_SETTINGS.set(...)`), not ad-hoc config.

## Required Validation

Run checks for every touched component:

- C++: build the affected target, run focused GoogleTests, format changed files.
- GUI: Dart format check, Flutter analyze, focused or full Flutter tests.
- Rust: `cargo fmt --check`, Clippy, focused or workspace tests.
- CLI: focused pytest with the correct controller and `--bin-dir` in standalone mode.

Also run the relevant registered CTest (or the full suite) when changing shared
contracts, platform interfaces, daemon lifecycle, or broad user-facing workflows.

Do not claim a check passed unless it was run. In the final report, name the checks
run and explain any relevant checks that could not be run.

## Security

- `multipassd` runs with elevated privileges. Treat all client, image, cloud-init,
  path, mount, network, and settings input as untrusted at process and filesystem
  boundaries.
- Never interpolate untrusted input into shell commands; prefer structured process
  arguments and the existing wrappers in `src/process/`.
- Changes under `src/cert/`, `src/ssh/`, authentication RPCs, and permission
  handling can affect daemon or instance access; preserve authentication and
  authorization checks and add negative tests.
- Do not log private keys, certificates, tokens, credentials, or sensitive
  cloud-init data.

## Git and Pull Requests

- Do not commit, create branches, rebase, or modify submodules unless explicitly
  asked.
- Keep changes and commits small and single-purpose; cover behavior with automated
  tests.
- Commit messages are validated by `git-hooks/commit-msg.py`:
  - Subject: starts with a lowercase, hyphen-composed category in brackets
    (e.g. `[daemon]`, `[bug-fix]`), a space, then a capitalized word; no trailing
    period; 50 characters max including the category, e.g. `[daemon] Fix startup
    race`.
  - Body (if present): separated from the subject by a blank line, wrapped at 72
    characters except blockquotes, `[n]:` references, `Signed-off-by:`, and
    `Co-authored-by:` lines; no consecutive blank lines.
  - `fixup!`/`squash!` prefixes are allowed for autosquash rebase workflows.
- Review the final diff and avoid whitespace errors. Follow `CONTRIBUTING.md` for
  full Git and PR policy.
