---
applyTo: "tests/**"
---

# Tests

These rules add to `AGENTS.md`, which covers how to build and run the suites. Match the fixture,
naming, and assertion style of nearby tests.

## C++ unit tests (`tests/unit/`)

- Name files `test_<area>.cpp` and add them to the explicit source list of `multipass_cpp_tests`
  in `tests/unit/CMakeLists.txt`. Platform- and backend-specific tests live in subdirectories
  (`linux/`, `macos/`, `windows/`, `qemu/`, `hyperv_api/`, ...) and are added with
  `target_sources(multipass_cpp_tests PRIVATE ...)` in that directory's `CMakeLists.txt`.
- Include `common.h` and use the `mpt` (`multipass::test`) helpers before writing new ones:
  `TempDir`, `TempFile`, `file_operations.h`, `json_test_utils.h`, `stub_*.h`, and
  `daemon_test_fixture.h` for daemon tests.
- Inject singleton mocks through the guard returned by `inject()`, which restores the real
  instance when it goes out of scope:

  ```cpp
  auto [mock_platform, guard] = mpt::MockPlatform::inject<NiceMock>();
  ```

- Choose `NiceMock` for collaborators the test does not verify and `StrictMock` where unexpected
  calls must fail. Use `EXPECT_CALL` for behavior under test and `ON_CALL` for defaults.
- Assert exceptions with `MP_EXPECT_THROW_THAT` / `MP_ASSERT_THROW_THAT` and
  `mpt::match_what(...)`; compare `QString`s with `mpt::match_qstring(...)`.
- Check logging with `mpt::MockLogger::inject()` and `expect_log(...)`; use `screen_logs()` to
  ignore unrelated log output.
- When an interface changes, update its `mock_*.h` in the same change.
- Cover error paths as well as the happy path. Avoid real sleeps and timing assumptions, and mock
  external processes with `mock_process_factory.h` instead of spawning them.

## CLI tests (`tests/cli/`)

- Name files `cli_<feature>_test.py`, group tests in classes, and tag them with a marker. New
  markers must be declared under `[tool.pytest.ini_options]` in `tests/cli/pyproject.toml`.
- Start the daemon with `@pytest.mark.usefixtures("multipassd")` (or
  `multipassd_class_scoped`), and get VMs from the `instance` fixture or the `launch()` context
  manager so they are cleaned up.
- Run CLI commands through the `multipass()` helper (`tests/cli/multipass/multipass_cmd.py`) and
  generate names with `random_vm_name()`. Reuse helpers in `tests/cli/multipass/` before adding
  new ones.
- Follow the isort (black profile), ruff, and pylint settings in `tests/cli/pyproject.toml`. CI
  does not enforce them, so match nearby code.
