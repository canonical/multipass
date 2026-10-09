---
applyTo: "src/client/gui/**"
---

# Flutter GUI

These rules add to `AGENTS.md`, which covers the vendored Flutter SDK, `pub get`, formatting,
analysis, and tests. Run Flutter commands from `src/client/gui`.

## Architecture

- State management is Riverpod (`flutter_riverpod`). Declare providers in `lib/providers.dart` or
  next to their feature, named `xxxProvider`. Use `Notifier`/`AsyncNotifier` classes for mutable
  state, with `.autoDispose` and `.family` as the surrounding code does.
- All daemon communication goes through `GrpcClient` (`lib/grpc_client.dart`). Do not create
  other RPC clients or cache daemon state outside providers.
- Native calls go through `lib/ffi.dart`, backed by `ffi/dart_ffi.cpp`. Changing an FFI function
  means updating both sides; the C++ side follows the C++ instructions.
- Platform differences go through `lib/platform/` (`LinuxPlatform`, `MacOSPlatform`,
  `WindowsPlatform`), not ad-hoc `Platform.isX` checks in widgets.
- Use `built_collection` types for collections held in provider state, and `fpdart` where the
  surrounding code already does.
- Log with the global `logger` from `lib/logger.dart`, not `print`.

## User-facing text

- Add new strings to `lib/l10n/app_en.arb` and read them with `AppLocalizations.of(context)!`.
  Do not hard-code user-facing text in widgets.
- `lib/l10n/app_localizations*.dart` is generated and gitignored; never edit or commit it.

## Generated and template code

- `lib/generated/` is produced from `src/rpc/multipass.proto` by `protoc` during the CMake GUI
  build. Change the proto, not the generated Dart.
- `linux/`, `macos/`, and `windows/` contain Flutter runner templates (clang-format is disabled
  in `linux/` and `windows/`). Change them only when the task requires it; the C++ instructions
  do not apply to runner code.

## Tests

- Put tests under `test/`, mirroring the `lib/` feature folders, with the `_test.dart` suffix.
- Replace dependencies with `ProviderScope(overrides: [...])` or a `ProviderContainer` with
  overrides. The project has no mocking package; prefer provider overrides to adding one.
