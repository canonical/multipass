#!/usr/bin/env python3

import hashlib
import json
import os
import subprocess
import sys
from contextlib import contextmanager
from pathlib import Path

import pytest

from cli.config import cfg
from cli.multipass import (
    get_cloudinit_instance_id,
    get_default_interface_name,
    get_mac_addr_of,
    launch,
    multipass,
    path_exists,
    read_file,
    snapshot_count,
    state,
    vm_exists,
    write_file,
)
from cli.utilities import uuid4_str

STOPPED_VM = "upg-hv-migrate"
RUNNING_VM = "upg-hv-running"
SUSPENDED_VM = "upg-hv-suspended"
BASE = "legacy-base"
HEAD = "legacy-head"
POST = "hcs-post"

pytestmark = pytest.mark.skipif(
    sys.platform != "win32" or cfg.driver != "hyperv",
    reason="Hyper-V migration verification requires Windows and the hyperv driver",
)


@contextmanager
def seeded_vm(name):
    if vm_exists(name):
        assert multipass("delete", name, "--purge")
    with launch(cfg_override={"name": name, "autopurge": False}) as vm:
        yield vm


def powershell(script):
    return subprocess.run(
        ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", script],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()


def ps_quote(value):
    return "'" + value.replace("'", "''") + "'"


def as_list(value):
    if value is None:
        return []
    return value if isinstance(value, list) else [value]


def legacy_layout(name):
    output = powershell(
        f"""
$vm = Get-VM -Name {ps_quote(name)} -ErrorAction Stop
$primary = @(Get-VMHardDiskDrive -VMName $vm.Name |
    Where-Object {{ $_.ControllerType -eq 'SCSI' -and
                    $_.ControllerNumber -eq 0 -and
                    $_.ControllerLocation -eq 0 }})
if ($primary.Count -ne 1) {{ throw 'Expected one primary disk' }}
$snapshots = @(Get-VMCheckpoint -VMName $vm.Name | ForEach-Object {{
    $disk = @(Get-VMHardDiskDrive -VMSnapshot $_ |
        Where-Object {{ $_.ControllerType -eq 'SCSI' -and
                        $_.ControllerNumber -eq 0 -and
                        $_.ControllerLocation -eq 0 }})
    if ($disk.Count -ne 1) {{ throw 'Expected one checkpoint disk' }}
    [PSCustomObject]@{{
        name = $_.Name
        id = $_.Id.ToString()
        path = $disk[0].Path
    }}
}})
[PSCustomObject]@{{
    id = $vm.Id.ToString()
    state = $vm.State.ToString()
    active_disk = $primary[0].Path
    snapshots = $snapshots
}} | ConvertTo-Json -Compress -Depth 4
"""
    )
    result = json.loads(output)
    result["snapshots"] = as_list(result.get("snapshots"))
    return result


def legacy_id_exists(vm_id):
    return (
        powershell(
            f"if (Get-VM -Id {ps_quote(vm_id)} -ErrorAction SilentlyContinue) "
            "{ 'true' } else { 'false' }"
        )
        == "true"
    )


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(4 * 1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def ownership_path(name):
    return Path(cfg.data_dir) / "vault" / "instances" / name / "hcs-ownership.json"


def sentinel(name, label):
    record = {
        "path": f"/home/ubuntu/upgrade-sentinel-{label}.txt",
        "content": f"upgrade-sentinel::{label}::{uuid4_str()}",
    }
    assert write_file(name, record["path"], record["content"])
    return record


def assert_sentinel(name, record):
    assert read_file(name, record["path"]).strip() == record["content"]


def identity(name):
    interface = get_default_interface_name(name)
    return {
        "hostname": multipass("exec", name, "--", "hostname").content.strip(),
        "machine_id": read_file(name, "/etc/machine-id").strip(),
        "ssh_host_key": read_file(name, "/etc/ssh/ssh_host_ed25519_key.pub").strip(),
        "cloud_init_id": get_cloudinit_instance_id(name).strip(),
        "mac": get_mac_addr_of(name, interface).strip().lower(),
    }


def assert_identity(name, expected):
    assert identity(name) == expected


def take_snapshot(name, snapshot_name):
    assert state(name) == "Stopped"
    assert multipass("snapshot", name, "--name", snapshot_name)


@pytest.mark.seed
@pytest.mark.snapshot
@pytest.mark.scenario(STOPPED_VM)
def test_stopped_snapshot_seed(scenario):
    with seeded_vm(STOPPED_VM):
        base = sentinel(STOPPED_VM, "hv-base")
        guest_identity = identity(STOPPED_VM)
        assert multipass("stop", STOPPED_VM)
        take_snapshot(STOPPED_VM, BASE)

        assert multipass("start", STOPPED_VM)
        head = sentinel(STOPPED_VM, "hv-head")
        assert multipass("stop", STOPPED_VM)
        take_snapshot(STOPPED_VM, HEAD)

    layout = legacy_layout(STOPPED_VM)
    scenario.record.update(
        {
            "legacy_id": layout["id"],
            "active_disk": layout["active_disk"],
            "snapshot_disks": {
                item["name"]: {"path": item["path"], "sha256": sha256(item["path"])}
                for item in layout["snapshots"]
            },
            "base": base,
            "head": head,
            "identity": guest_identity,
            "snapshot_count": snapshot_count(STOPPED_VM),
        }
    )


@pytest.mark.verify
@pytest.mark.snapshot
@pytest.mark.scenario(STOPPED_VM)
def test_stopped_snapshot_verify(scenario):
    record = scenario.record
    assert state(STOPPED_VM) == "Stopped"
    assert legacy_id_exists(record["legacy_id"])
    assert multipass("start", STOPPED_VM, timeout=900)
    assert_sentinel(STOPPED_VM, record["base"])
    assert_sentinel(STOPPED_VM, record["head"])
    assert_identity(STOPPED_VM, record["identity"])
    assert not legacy_id_exists(record["legacy_id"])

    migration = json.loads(ownership_path(STOPPED_VM).read_text(encoding="utf-8"))
    assert migration["backend"] == "hcs"
    assert os.path.normcase(migration["active_disk"]) == os.path.normcase(record["active_disk"])
    assert Path(migration["state_file_stem"]).name == "hcs-migrated-state"
    for checkpoint, disk in record["snapshot_disks"].items():
        index = int(checkpoint.removeprefix("@s"))
        metadata_path = ownership_path(STOPPED_VM).with_name(
            f"{index:04}.snapshot.json"
        )
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        migrated_path = metadata["snapshot"]["disk_path"]
        assert os.path.normcase(migrated_path) == os.path.normcase(disk["path"])
        assert sha256(disk["path"]) == disk["sha256"]

    assert multipass("stop", STOPPED_VM)
    take_snapshot(STOPPED_VM, POST)
    assert snapshot_count(STOPPED_VM) == record["snapshot_count"] + 1

    assert multipass("restore", f"{STOPPED_VM}.{BASE}", "--destructive")
    assert multipass("start", STOPPED_VM)
    assert_sentinel(STOPPED_VM, record["base"])
    assert not path_exists(STOPPED_VM, record["head"]["path"])

    assert multipass("stop", STOPPED_VM)
    assert multipass("restore", f"{STOPPED_VM}.{HEAD}", "--destructive")
    assert multipass("start", STOPPED_VM)
    assert_sentinel(STOPPED_VM, record["head"])


@pytest.mark.seed
@pytest.mark.scenario(RUNNING_VM)
def test_running_seed(scenario):
    with seeded_vm(RUNNING_VM):
        scenario.record.update(
            {
                "legacy_id": legacy_layout(RUNNING_VM)["id"],
                "sentinel": sentinel(RUNNING_VM, "hv-running"),
                "identity": identity(RUNNING_VM),
            }
        )


@pytest.mark.verify
@pytest.mark.scenario(RUNNING_VM)
def test_running_verify(scenario):
    record = scenario.record
    assert state(RUNNING_VM) == "Running"
    assert legacy_id_exists(record["legacy_id"])
    assert_sentinel(RUNNING_VM, record["sentinel"])
    assert multipass("stop", RUNNING_VM)
    assert multipass("start", RUNNING_VM, timeout=900)
    assert not legacy_id_exists(record["legacy_id"])
    assert_identity(RUNNING_VM, record["identity"])


@pytest.mark.seed
@pytest.mark.scenario(SUSPENDED_VM)
def test_suspended_seed(scenario):
    with seeded_vm(SUSPENDED_VM):
        record = {
            "sentinel": sentinel(SUSPENDED_VM, "hv-suspended"),
            "identity": identity(SUSPENDED_VM),
        }
        assert multipass("suspend", SUSPENDED_VM)
    record["legacy_id"] = legacy_layout(SUSPENDED_VM)["id"]
    scenario.record.update(record)


@pytest.mark.verify
@pytest.mark.scenario(SUSPENDED_VM)
def test_suspended_verify(scenario):
    record = scenario.record
    assert state(SUSPENDED_VM) == "Suspended"
    assert legacy_id_exists(record["legacy_id"])
    assert multipass("start", SUSPENDED_VM)
    assert legacy_id_exists(record["legacy_id"])
    assert_sentinel(SUSPENDED_VM, record["sentinel"])
    assert multipass("stop", SUSPENDED_VM)
    assert multipass("start", SUSPENDED_VM, timeout=900)
    assert not legacy_id_exists(record["legacy_id"])
    assert_identity(SUSPENDED_VM, record["identity"])
