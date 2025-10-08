# Multipass CLI tests

Multipass CLI tests test the user-facing CLI functionality by using `multipass` like a user would do.

The tests are divided into separate suites based on the functionality/command.

## Installing the tests

The tests are located in the Multipass repository, and do not require anything from the repository except for itself, i.e. the `tools/cli_tests` folder.

All the runtime dependencies for the tests are listed in the `pyproject.toml` file, under `project.dependencies`. The following command installs the dependencies:

```bash
sudo pip install -e ./tools/cli_tests/ --break-system-packages
```

## To run

The tests support running on installed versions of Multipass on Linux (snap), macOS (package), Windows (MSI package). Additionally, the tests also support running on a source-built version of Multipass, which is referred to as `standalone` mode.

The test suite has dedicated platform-specific code for interacting with the installed Multipass. These are referred as `daemon controller`s in the Multipass CLI test terminology. There are four daemon controllers at the moment, namely:

- `snap`: Interacts with the snap-installed Multipass for test governance
- `winsvc`: Windows Service based (Windows MSI Package)
- `launchd`: Launchd-based installations (macOS package)
- `standalone`: Source-built version of the Multipass, either installed to an arbitrary location, or in a build folder

By default, the CLI tests would pick the appropriate daemon controller for the platform, i.e. `snap` for Linux, `winsvc` for Windows, `launchd` for macOS. It's possible to override this explicitly by using the `--daemon-controller` flag.

Below are example test run invocations for each platform (hence for each daemon controller).

### Linux, snap

```bash
pytest tools/cli_tests # or pass `--daemon-controller=snap` explicitly
```

### macOS, launchd

```bash
pytest tools/cli_tests # or pass `--daemon-controller=launchd` explicitly
```

### Windows, Windows Service

```bash
pytest tools/cli_tests # or pass `--daemon-controller=winsvc` explicitly
```

### Standalone (all platforms)

Standalone mode needs the additional `--bin-dir` option as it needs to know where to look for the `multipassd` and `multipass` binaries. The path must contain both.

```bash
pytest tools/cli_tests/ --daemon-backend=standalone --bin-dir=build/bin
```

By default, the `standalone` mode uses a temporary directory (e.g. `/tmp/tmp2pwysbvk`) as the `MULTIPASS_STORAGE_DIR`, which would be automatically removed after the test session ends. To override this, `--storage-dir` option can be used. The directory contents are **not removed** after the test session when the `--storage-dir` is explicitly specified. This can be used for saving time for repeated testing sessions.

### Running tests selectively

All suites have a dedicated `pytest.mark` attached to them. All the markers are listed in the `pyproject.toml/tool.pytest.ini_options.markers` section. They can also be listed by using the pytest's `--markers` command:

```bash
pytest tools/cli_tests --markers

# @pytest.mark.snapshot: Snapshot tests.
# @pytest.mark.clone: Clone tests.
# ...
pytest tools/cli_tests -m snapshot # only run snapshot tests
```

Another way is to be specific about test file to run:

```bash
pytest tools/cli_tests/cli_version_test.py

# cli_version_test.py ✓✓                                                 100% ██████████
#
# Results (24.91s):
#       2 passed
```

Also, it's possible to use pytest keyword expression by `-k`:

```bash
pytest -s tools/cli_tests "shell_test"

# collected 310 items / 309 deselected / 1 selected
#
# cli_shell_test.py ✓                                                    100% ██████████
#
# Results (161.59s (0:02:41)):
#       1 passed
#     309 deselected
```

See [pytest](https://docs.pytest.org/en/6.2.x/usage.html)'s documentation about [Specifying tests / selecting tests](https://docs.pytest.org/en/6.2.x/usage.html#specifying-tests-selecting-tests) to learn more.

### Repeating the failed tests

Append `--last-failed` to the test run command, like as follows:

```bash
pytest tools/cli_tests --last-failed
```

The runner will only run the tests that failed in the last test run.

### Increasing verbosity

By default, the tests run silently and only report the PASS/FAIL status, unless a failure happens. The following additional flags can be used to selectively increase the verbosity of the output from the test run:

#### `--print-cli-output`

Prints the output from the `multipass` command as the tests being executed.

#### `--print-daemon-output`

Prints the output from the `multipassd`, the multipass daemon.

#### `--print-all-output`

Shorthand for `--print-cli-output --print-daemon-output`.

#### `--log-level`

The test code also utilizes `logging` to provide additional details. This can be enabled via setting the pytest's log level:

```bash
pytest tools/cli_tests/ --log-level=DEBUG # INFO, TRACE, WARN..

# DEBUG    root:conftest.py:329 store_config :: final config namespace(vm=namespace(cpus='2', memory='1G', disk='6G', image='noble'), retries=namespace(launch=3), timeouts=namespace(launch=600, stop=180, mount=180, restart=120, delete=90, exec=90, start=90, umount=45), bin_dir='/snap/bin', storage_dir='/var/snap/multipass/common/data/multipassd', print_daemon_output=False, print_cli_output=False, remove_all_instances=False, driver='qemu', daemon_controller='snap', data_dir='/var/snap/multipass/common/data/multipassd')
# DEBUG    root:conftest.py:418 environment_setup :: setup
# DEBUG    asyncio:selector_events.py:64 Using selector: EpollSelector
# DEBUG    root:multipassd_governor.py:219 multipassd-governor :: stop called
# DEBUG    root:multipassd_governor.py:163 multipassd-governor :: start called
# DEBUG    root:privutils.py:79 depth: 2
# DEBUG    root:privutils.py:93 module.__file__ = /workspace/multipass/tools/cli_tests/multipass/certutils.py
# DEBUG    root:privutils.py:94 inferred PYTHONPATH = /workspace/multipass/tools
# DEBUG    root:multipassd_governor.py:64 multipassd-governor :: read_stream start
# DEBUG    root:multipassd_governor.py:301 b'find failed: cannot connect to the multipass socket\n'
# DEBUG    root:multipassd_governor.py:124 multipassd-governor :: monitor task exited (cancelled: False)
# DEBUG    root:multipassd_governor.py:219 multipassd-governor :: stop called
# [DEBUG] environment_setup :: teardown
```

### Other options

#### `--driver` option

By default, the CLI tests use the default driver for the platform. The `--driver` option can be used for testing different backends available in the platform.

```bash
pytest tools/cli_tests/ --daemon-backend=snapd --driver=lxd # Test the `lxd` backend on Linux
```

```bash
pytest tools/cli_tests/ --daemon-backend=winsvc --driver=virtualbox # Test the `virtualbox` backend on Windows
```

#### `--remove-all-instances` flag

Additionally, there's a convenience flag called `--remove-all-instances`. The test executor would remove the instance storage directories (including backend-specific ones), like `vault/instances`, `qemu/vault/instances`, `virtualbox/vault/instances` while keeping the image cache and other things intact. This would allow reuse of a `MULTIPASS_STORAGE_DIR` without having adverse effects on future test runs due to persisted instances.

**Caution:** This flag will remove all the instances you have in the `MULTIPASS_STORAGE_DIR` for the daemon controller.

```bash
pytest tools/cli_tests/cli_tests.py --bin-dir build/bin/ --storage-dir=/tmp/multipass-test --remove-all-instances # would remove all instances in the `/tmp/multipass-test` before the test session.
```

### Setting default resources and parameters for the test VM

By default, the default VM is a `noble` VM with `6G` disk, `1G` memory and `2` cores. All of these parameters can be overridden by specifying `--vm-<option>` parameters listed below.

- `--vm-cpus`: CPU core count
- `--vm-memory`: Memory size
- `--vm-disk`: Disk size
- `--vm-image`: Image

### Timeout and retry attempt counts for commands

All commands have certain timeout values and retry attempts associated with them to provide a stable testing experience. This is to not fail when a certain command may fail a couple of times before succeeding (e.g. `launch` may fail while the daemon is starting), and it's considered fine. While the test code preemptively and defensively tries to remedy these, the `timeout` and `retry` attempts are still useful as a safety net.

#### Timeout values and `--cmd-timeout`

The `timeout` is how long to wait for a certain command to complete. This is enforced at process level and independent from the `--timeout` value the `multipass <command>` has. The timeout values are set on per command basis and each command can have a different timeout value (e.g., `600s` for launch, `60s`for `stop`.) The timeout is applied to per command invocation, so if there are `retries` each retry would get the timeout independently.

The defaults are listed below:

| Command              | Default timeout value |
| -------------------- | --------------------- |
| launch               | 600s                  |
| stop                 | 180s                  |
| mount                | 180s                  |
| restart              | 120s                  |
| delete               | 90s                   |
| exec                 | 90s                   |
| start                | 90s                   |
| umount               | 45s                   |
| <all other commands> | 30s                   |

`--cmd-timeouts` parameter can be used to change the defaults as follows:

```bash
pytest tools/cli_tests/ --cmd-timeouts launch=300 start=180 alias=10
```

#### Retry attempts and `--cmd-retries`

The `retry` is how many times a certain command is going to be `retried` on failure until it's considered a failure. This is to prevent things like `launch` sometimes fails on first invocation because the daemon is not ready yet.

Right now, only the `launch` command has `3` retry attempts by default, while all other commands has `0`.

`--cmd-retries` parameter can be used to change the default as follows:

``` bash
pytest tools/cli_tests/ --cmd-retries launch=5 exec=2
```

### Remarks

It's possible to do plethora of things with `pytest` flags. Refer to the [pytest's documentation](https://docs.pytest.org/en/6.2.x/usage.html) to learn more about how to meet your particular needs.

## Running CLI Tests in GitHub Actions

The CLI tests has a dedicated workflow in `.github/workflows/cli-tests.yaml` path. This workflow can be triggered either via `workflow_call` (i.e., from another workflow), or `workflow_dispatch` (i.e., manually). The workflow dispatch can be used as following:

```yaml
  DispatchCLITestsWorkflow:
    # The cli-tests.yaml workflow needs checks: write to write test reports.
    permissions:
      contents: read
      actions: read
      checks:  write
    secrets: inherit
    uses: ./.github/workflows/cli-tests.yml
    with:
      macos-pkg-url: https://example.com/macos/multipass.pkg
      windows-pkg-url: https://example.com/win/multipass.msi
      snap-channel: edge/pr1234
```

The `macos-pkg-url`, `windows-pkg-url` and `snap-channel` will determine the matrix to be dispatched. The platform will be skipped when the respective variable is unset.

The `pytest-extra-args` can be used to pass additional arguments to the pytest run, to e.g. enabe additional debug output, or run a specific set of tests instead of the full test harness.

The manual dispatch can be made through GitHub Web UI, GitHub API, or GitHub CLI. Below is an example how to trigger the cli-tests from GitHub CLI:

```sh
 # This will run the CLI tests against the snap edge channel only.
 gh workflow run -r feature/cli-tests "CLI Tests" -f snap-channel=edge
 # This will run the CLI tests against the Windows MSI package, and the macOS package.
 gh workflow run -r feature/cli-tests "CLI Tests" -f windows-pkg-url=https://multipass-ci.s3.amazonaws.com/pr4061/multipass-1.17.0-dev.550.pr4061%2Bg0239bc1f.win-win64.msi macos-pkg-url=https://multipass-ci.s3.amazonaws.com/pr4061/multipass-1.17.0-dev.550.pr4061%2Bg0239bc1f.mac-Darwin.pkg pytest-extra-args="--print-all-output"
```

## Test coverage status

The suite has a good coverage of all existing Multipass except for a few. The table below summarizes the test coverage status for per-command:

| Command      | Category                   | Covered in suite                         |
| ------------ | -------------------------- | ---------------------------------------- |
| alias        | Interaction                | cli_alias_test.py                        |
| aliases      | Interaction                | cli_alias_test.py                        |
| unalias      | Interaction                | cli_alias_test.py                        |
| shell        | Interaction                | cli_shell_test.py                        |
| exec         | Interaction                | Tested indirectly                        |
| prefer       | Interaction                | cli_alias_test.py                        |
| info         | Introspection              | Tested indirectly                        |
| list         | Introspection              | cli_list_test.py, also tested indirectly |
| launch       | Provisioning and Lifecycle | cli_launch_test.py                       |
| start        | Provisioning and Lifecycle | cli_vm_lifecycle_test.py                 |
| stop         | Provisioning and Lifecycle | cli_vm_lifecycle_test.py                 |
| restart      | Provisioning and Lifecycle | cli_vm_lifecycle_test.py                 |
| delete       | Provisioning and Lifecycle | cli_vm_lifecycle_test.py                 |
| purge        | Provisioning and Lifecycle | cli_vm_lifecycle_test.py                 |
| recover      | Provisioning and Lifecycle | cli_vm_lifecycle_test.py                 |
| mount        | File I/O                   | cli_mount_test.py                        |
| umount       | File I/O                   | cli_mount_test.py                        |
| transfer     | File I/O                   | cli_transfer_test.py                     |
| clone        | Clone                      | cli_clone_test.py                        |
| restore      | Snapshot                   | cli_snapshot_test.py                     |
| snapshot     | Snapshot                   | cli_snapshot_test.py                     |
| authenticate | IAM                        | cli_authenticate_test.py                 |
| find         | Image Metadata             | cli_find_test.py                         |
| get          | Configuration              | cli_settings_test.py                     |
| set          | Configuration              | cli_settings_test.py                     |
| help         | Help                       | cli_help_test.py                         |
| version      | Help                       | cli_version_test.py                      |
| networks     | Networking                 | N/A                                      |
| wait-ready   |                            | N/A                                      |

## Useful commands

```bash
# kill zombie dnsmasq
ps -eo pid,cmd | awk '/dnsmasq/ && /mpqemubr0/ { print $1 }' | xargs sudo kill -9

# kill multipassd
sudo kill -TERM $(pidof multipassd)
```

## Best practices

- If a test case takes >1m to complete, mark it with `@pytest.mark.slow`

## `TL; DR;`

```bash
sudo apt install python3-pip
# Install the latest stable multipass
sudo snap install multipass
# Clone the feature branch only
git clone https://github.com/canonical/multipass.git --single-branch --branch feature/cli-tests
cd multipass/
# Installs all the deps needed for the tests
pip install -e ./tools/cli_tests/ --break-system-packages
# Run the tests!
pytest -s tools/cli_tests --remove-all-instances --print-all-output --daemon-controller=snap -vv
```
