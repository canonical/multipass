---
applyTo: "src/**/*.{h,hpp,cpp,c,mm},include/**/*.{h,hpp},tests/**/*.{h,cpp}"
---

# C++ code

These rules add to `AGENTS.md`, which covers build, formatting, and validation. Rule IDs refer
to `CONTRIBUTING.md`. They do not apply to the Flutter runner templates under
`src/client/gui/{linux,macos,windows}/`.

## Types and initialization

- Make copyable types semiregular (CPP6).
- Types that must not be copied or moved inherit `DisabledCopyMove` and only have constructors
  that fully initialize the object; add a default constructor only when no parameters are needed
  (CPP4, CPP5, CPP7).
- Catch misuse at compile time where possible (CPP3).
- Do not use unconstrained `auto` return types in publicly visible functions. Exceptions: tests,
  the anonymous namespace, unnamed lambdas, and templates with dependent return types (CPP15).
- Put generic constants in `include/multipass/constants.h` or a dedicated header instead of
  using magic numbers (CPP11, CPP12).

## Errors and logging

- Report errors with exceptions (CPP16). Reuse a type from `include/multipass/exceptions/` before
  adding a new one; `FormattedExceptionBase` supports fmt-style messages.
- When a backend cannot support an operation, throw `NotImplementedOnThisBackendException`.
- Log through `multipass::logging` with a category and an fmt format string, for example
  `mpl::info(vm_name, "Starting {}", vm_name)`.

## Mockable dependencies

- Reach system services through the existing singletons (`MP_FILEOPS`, `MP_PLATFORM`,
  `MP_UTILS`, `MP_SETTINGS`, `MP_STDPATHS`, and others defined in `include/multipass/`) instead
  of calling Qt or OS APIs directly, so tests can inject mocks.
- To wrap a new free function or external API, derive from `Singleton<T>`
  (`include/multipass/singleton.h`), expose an `MP_*` accessor macro, and add a mock under
  `tests/unit/` using `MP_MOCK_SINGLETON_BOILERPLATE` (CPP14).

## Platforms and backends

- Platform-conditional code (`#ifdef`, OS checks) belongs only in dedicated platform units,
  usually under `src/platform/` (CPP9). Shared code calls `MP_PLATFORM` or an interface.
- Changes to `VirtualMachine` or `VirtualMachineFactory` behavior must cover every backend in
  `src/platform/backends/`, or justify the gap. Put common logic in `shared/` base classes rather
  than duplicating it per backend.

## Concurrency and lifetimes

- Keep lock scopes minimal and use the predicate overload of `condition_variable::wait`.
- Respect Qt thread affinity: cross threads with signals and queued connections, not direct
  calls on another thread's `QObject`.
- Ensure callbacks and lambdas cannot outlive the objects they capture.

## New files

- Start new files with the Canonical GPLv3 license header used by nearby files.
- Source files are listed explicitly in each `CMakeLists.txt`; add new files to the matching
  target.
