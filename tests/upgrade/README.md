# Version upgrade tests

The version upgrade suite checks that VMs and their state created on one Multipass version survive an upgrade to another. It lives at `tests/upgrade` (a sibling of `tests/cli`) and is its own entry point, run with `pytest tests/upgrade`. Because it is not under `tests/cli`, the standard `pytest tests/cli` run never collects it.

It nonetheless reuses the *entire* cli framework. Its `conftest.py` sets `pytest_plugins = ["cli.conftest"]`, which pulls in the framework's daemon controllers, the `multipassd` governor fixture, the sudo/binary session setup, and the shared `--driver` / `--daemon-controller` / `--vm-*` options. The `cli` package itself is importable because pytest puts `tests/` on `sys.path` (both `tests/cli` and `tests/upgrade` are packages; `tests` is not). The suite's own `conftest.py` adds only the upgrade-specific options and the manifest fixture, and a small `tests/upgrade/pytest.ini` registers the `upgrade` marker.

Because `pytest_plugins` must sit in a top-level conftest, invoke the suite via its own path (`pytest tests/upgrade`) rather than as part of a combined `pytest tests` run.

## The two-phase model

Upgrade testing is split into two phases, run as separate `pytest` invocations selected by marker, with a package upgrade in between:

1. **`-m seed`** — provisions test VMs on the *currently installed* version (X), drives each into a known state, and records what should still hold afterwards into an on-disk **manifest**.
2. **(upgrade)** — the installed Multipass is swapped from X to Y. This is orchestrated by the CI (or by you, manually); the test code never installs anything.
3. **`-m verify`** — reads the manifest back and asserts every recorded expectation still holds on version Y.

Two constraints follow from how Multipass stores state:

- **Both phases must run on the same host.** Seeded VMs live in the daemon's storage directory (e.g. `/var/snap/multipass/common/data/multipassd`), which does not travel between machines. The two `pytest` runs are sequential steps in a single CI job, not two jobs.
- **Never pass `--remove-all-instances`.** That flag wipes instance storage — including the very VMs the verify phase depends on.

The `verify` phase derives nothing from the seed run's process state; VM names and all expectations flow through the manifest, which is the single source of truth.

## Running locally

Assuming a snap-installed Multipass and the test dependencies installed (see [Installing the tests](#installing-the-tests)):

```bash
# Phase 1 — seed on the currently installed version
pytest tests/upgrade -m seed \
    --upgrade-manifest=reports/upgrade-manifest.json

# Upgrade the installed package, keeping storage in place
sudo snap refresh multipass --channel=<target-channel>

# Phase 2 — verify on the upgraded version
pytest tests/upgrade -m verify \
    --upgrade-manifest=reports/upgrade-manifest.json
```

Both runs must use the same `--upgrade-manifest` path and the same `--driver`. The phase is chosen with the `seed` / `verify` markers; the seed run writes the manifest, the verify run reads it. `--upgrade-manifest` lives in the suite's own conftest, so it is only available when you run `pytest tests/upgrade`, not on the `pytest tests/cli` run.

### Options

- `-m seed` / `-m verify`: select the phase. Run one, upgrade the package, then run the other.
- `--upgrade-manifest=PATH`: location of the JSON manifest carrying expectations between phases. Defaults to `upgrade-manifest.json` in the working directory.

## The manifest

The manifest is a small, human-readable JSON document. It captures the seed environment and maps each VM name to the expectations the seed test recorded:

```json
{
  "schema": 1,
  "seed": { "version": "1.16.0", "driver": "qemu", "controller": "snap" },
  "scenarios": {
    "upg-lifecycle": { "sentinel_path": "...", "fingerprint": { "...": "..." } }
  }
}
```

It is attached as a CI artifact (`upgrade-manifest`) so a failed verify run can be diffed against what was seeded.

## The tests

Each concern is a pair of ordinary pytest tests in `tests/upgrade/<concern>_test.py`: a `seed` test (marked `@pytest.mark.seed`) and a `verify` test (marked `@pytest.mark.verify`). Both name their VM once with `@pytest.mark.scenario("<vm-name>")` and request a fixture instead of indexing the manifest by hand: the seed test launches the VM, drives it into a state, and records what should survive into `seed_scenario.record`; the verify test reads `verify_scenario.record` and asserts. The `verify_scenario` fixture purges the VM in teardown (even on failure), so cleanup can't be forgotten.

| Test | What it asserts survives |
| ---- | ------------------------ |
| `lifecycle` | A stopped VM's on-disk data and host-reported identity (cpu count, memory total, image release); it still boots. |
| `suspend_resume` | A VM **suspended** during seeding comes back `Suspended` and resumes cleanly with its data. Skipped on `lxd`/`applevz`. |
| `snapshot` | A two-level snapshot tree: its metadata (names, parents, comments, count) and the data captured in each snapshot (restoring `base` reveals exactly the state live when it was taken). Skipped on `lxd`/`applevz`. |
| `mount` | A classic mount: the definition (still listed in `info`), host-written data visible in the guest, and guest-written data still on the host and re-exposed once the VM restarts. |
| `cloudinit` | State applied by a custom `--cloud-init` at first boot: the provisioned file persists byte-for-byte and the cloud-init instance id is unchanged (proving it did not re-run). |
| `network` | An extra `--network` interface: the guest still presents it with the same persisted MAC. Each phase stands up its own isolated, runtime-only host network and tears it down afterwards (privilege escalated via `sudo`/`gsudo`): a **bridge** on Linux (`ip link`), a **private Hyper-V vSwitch** on Windows (`New-VMSwitch -SwitchType Private`). It is re-created with the same fixed name each phase, so the instance's persisted NIC (MAC + network reference) re-attaches by name on verify. **Skipped on macOS** — both backends only bridge onto physical NICs, so an isolated ephemeral network can't be created there. |

Tests deliberately park VMs in a `Stopped` (or `Suspended`) state at the end of seeding so the verify phase has a precise expectation rather than a version-dependent one (whether a *running* VM is re-attached or stopped by a refresh is daemon/version specific).

### Adding a test

Drop a `<concern>_test.py` in `tests/upgrade/` with a `seed`-marked test that records into `seed_scenario.record` and a `verify`-marked test that reads it back from `verify_scenario.record`; tag both with `@pytest.mark.scenario("<vm-name>")` (the `verify_scenario` fixture then purges the VM for you). Shared assertions live in `helpers.py` — including `park_seeded`/`resume_seeded` (the symmetric "park the VM at end of seed" / "bring it back in verify" pair) and `seed_sentinel`/`assert_sentinel_record` (write a labelled guest file and later compare it byte for byte). `seedutils.py` has `seeded_vm` (the idempotent, persistent-VM launch wrapper), `ensure_absent`, and `daemon_readable_dir` (a snap-readable, refresh-surviving host dir). Launch persistent seed VMs with `seeded_vm("<vm-name>", extra_args=[...])`, which wraps the cli `launch` helper with `autopurge=False`. Driver-gated concerns reuse the cli framework's markers (e.g. `@pytest.mark.snapshot` is auto-skipped on drivers without snapshot support).

## Running in GitHub Actions

The upgrade suite has its own workflow at `.github/workflows/version-upgrade-tests.yml`, triggerable via `workflow_call` or `workflow_dispatch`. It installs the `from` channel, seeds, `snap refresh`es to the `to` channel, then verifies — all in one job, reporting both phases to GitHub Checks.

```sh
# Validate that VMs seeded on stable survive an upgrade to the candidate channel.
gh workflow run "Version Upgrade Tests" \
    -f from-snap-channel=stable \
    -f to-snap-channel=candidate
```

The `backend-driver` and `pytest-extra-args` inputs are forwarded to both phases.

## TODO: Add deleted vm
