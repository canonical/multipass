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

STOPPED_VM = "upg-hcs-phase1-stopped"
RUNNING_VM = "upg-hcs-phase1-running"
SUSPENDED_VM = "upg-hcs-phase1-suspended"
DELETED_VM = "upg-hcs-phase1-deleted"
SCENARIO = STOPPED_VM
VM_NAMES = (STOPPED_VM, RUNNING_VM, SUSPENDED_VM, DELETED_VM)
RECORD_KEY = {
    STOPPED_VM: "stopped",
    RUNNING_VM: "running",
    SUSPENDED_VM: "suspended",
    DELETED_VM: "deleted",
}
BASE = "legacy-base"
HEAD = "legacy-head"
POST = "hcs-post"
MOUNT_TARGET = "/home/ubuntu/upgrade-mount"

pytestmark = pytest.mark.skipif(
    sys.platform != "win32" or getattr(cfg, "driver", None) != "hyperv",
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
    return "'" + str(value).replace("'", "''") + "'"


def as_list(value):
    if value is None:
        return []
    return value if isinstance(value, list) else [value]


def disk_chain(path):
    output = powershell(
        f"""
$current = {ps_quote(path)}
$chain = @()
while ($current) {{
    $chain += $current
    $current = (Get-VHD -Path $current -ErrorAction Stop).ParentPath
}}
$chain | ConvertTo-Json -Compress
"""
    )
    return as_list(json.loads(output)) if output else []


def legacy_layout(name, include_disks=True):
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

    result["disks"] = []
    if include_disks:
        seen = set()
        roots = [result["active_disk"], *(item["path"] for item in result["snapshots"])]
        for root in roots:
            for disk in disk_chain(root):
                key = os.path.normcase(os.path.abspath(disk))
                if key not in seen:
                    seen.add(key)
                    result["disks"].append(disk)
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


def file_record(path):
    path = Path(path)
    return {"path": str(path), "size": path.stat().st_size, "sha256": sha256(path)}


def assert_file_records_unchanged(records):
    for record in records:
        path = Path(record["path"])
        assert path.exists()
        assert path.stat().st_size == record["size"]
        assert sha256(path) == record["sha256"]


def legacy_instance_dir(name):
    return Path(cfg.data_dir) / "vault" / "instances" / name


def target_instance_dir(name):
    return Path(cfg.data_dir) / "hyperv_api" / "vault" / "instances" / name


def ownership_path(name):
    return target_instance_dir(name) / "hcs-ownership.json"


def database(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def legacy_vm_records():
    return database(Path(cfg.data_dir) / "multipassd-vm-instances.json")


def legacy_image_records():
    return database(
        Path(cfg.data_dir) / "vault" / "multipassd-instance-image-records.json"
    )


def target_vm_records():
    return database(
        Path(cfg.data_dir) / "hyperv_api" / "multipassd-vm-instances.json"
    )


def target_image_records():
    return database(
        Path(cfg.data_dir)
        / "hyperv_api"
        / "vault"
        / "multipassd-instance-image-records.json"
    )


def path_is_within(path, root):
    path = os.path.normcase(os.path.abspath(path))
    root = os.path.normcase(os.path.abspath(root))
    return os.path.commonpath([root, path]) == root


def current_driver():
    result = multipass("get", "local.driver", timeout=30, retry=30)
    assert result, f"Could not read local.driver: {result}"
    return result.content.strip()


def switch_driver(driver, governor):
    result = multipass("set", f"local.driver={driver}", timeout=900)
    assert result, f"Could not switch to {driver}: {result}"
    governor.wait_for_restart(timeout=600)
    assert current_driver() == driver
    return result


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


def source_record(name, layout, guest_identity=None, record_files=True):
    instance_dir = legacy_instance_dir(name)
    metadata_files = [
        path
        for path in (
            instance_dir / "cloud-init-config.iso",
            instance_dir / "snapshot-head",
            instance_dir / "snapshot-count",
            *sorted(instance_dir.glob("*.snapshot.json")),
        )
        if path.exists()
    ]
    return {
        "legacy_id": layout["id"],
        "active_disk": layout["active_disk"],
        "disks": [file_record(path) for path in layout["disks"]]
        if record_files
        else [],
        "metadata": [file_record(path) for path in metadata_files]
        if record_files
        else [],
        "identity": guest_identity,
    }


def assert_target_local(name, source):
    instance_dir = target_instance_dir(name)
    ownership = database(ownership_path(name))
    assert ownership["backend"] == "hcs"

    active_disk = Path(ownership["active_disk"])
    state_file_stem = Path(ownership["state_file_stem"])
    for path in (active_disk, state_file_stem):
        assert path_is_within(path, instance_dir)

    snapshot_disks = []
    for metadata_path in instance_dir.glob("*.snapshot.json"):
        metadata = database(metadata_path)
        snapshot_disks.append(Path(metadata["snapshot"]["disk_path"]))

    target_disks = []
    seen = set()
    for root in (active_disk, *snapshot_disks):
        for disk in disk_chain(root):
            key = os.path.normcase(os.path.abspath(disk))
            if key not in seen:
                seen.add(key)
                target_disks.append(Path(disk))

    assert len(target_disks) == len(source["disks"])
    assert sorted(path.stat().st_size for path in target_disks) == sorted(
        record["size"] for record in source["disks"]
    )
    for path in target_disks:
        assert path_is_within(path, instance_dir)

    assert (instance_dir / "cloud-init-config.iso").exists()
    assert not (instance_dir / "migration-transaction.json").exists()


def assert_target_records(name, expected_vm, expected_image):
    actual_vm = target_vm_records()[name]
    actual_image = target_image_records()[name]
    assert actual_vm == expected_vm

    actual_image_without_path = json.loads(json.dumps(actual_image))
    expected_image_without_path = json.loads(json.dumps(expected_image))
    actual_image_without_path["image"].pop("path")
    expected_image_without_path["image"].pop("path")
    assert actual_image_without_path == expected_image_without_path
    assert path_is_within(
        actual_image["image"]["path"], target_instance_dir(name)
    )


def assert_first_migration_output(output):
    assert "The following instances were successfully migrated" in output.content
    assert f"  {STOPPED_VM}" in output.content
    assert f"Cannot migrate {RUNNING_VM}: instance is running" in output.content
    assert f"Cannot migrate {SUSPENDED_VM}: instance is suspended" in output.content
    assert f"Cannot migrate {DELETED_VM}: instance is deleted" in output.content
    assert "Do not run an original and its hyperv_api copy at the same time" in output.content


@pytest.mark.seed
@pytest.mark.snapshot
@pytest.mark.scenario(SCENARIO)
def test_hyperv_migration_seed(scenario):
    assert current_driver() == "hyperv"

    mount_source = Path(cfg.storage_dir) / "hyperv-migration-mount"
    mount_source.mkdir(parents=True, exist_ok=True)
    mount_file = mount_source / "payload.txt"
    mount_content = f"host-mount::{uuid4_str()}"
    mount_file.write_text(mount_content, encoding="utf-8")

    with seeded_vm(STOPPED_VM):
        base = sentinel(STOPPED_VM, "hv-base")
        stopped_identity = identity(STOPPED_VM)
        assert multipass("mount", mount_source, f"{STOPPED_VM}:{MOUNT_TARGET}")
        assert read_file(STOPPED_VM, f"{MOUNT_TARGET}/payload.txt").strip() == mount_content
        assert multipass("stop", STOPPED_VM)
        take_snapshot(STOPPED_VM, BASE)

        assert multipass("start", STOPPED_VM)
        head = sentinel(STOPPED_VM, "hv-head")
        assert multipass("stop", STOPPED_VM)
        take_snapshot(STOPPED_VM, HEAD)

    stopped_layout = legacy_layout(STOPPED_VM)

    with seeded_vm(RUNNING_VM):
        running_sentinel = sentinel(RUNNING_VM, "hv-running")
        running_identity = identity(RUNNING_VM)
        assert multipass("stop", RUNNING_VM)
    running_layout = legacy_layout(RUNNING_VM, include_disks=False)

    with seeded_vm(SUSPENDED_VM):
        suspended_sentinel = sentinel(SUSPENDED_VM, "hv-suspended")
        suspended_identity = identity(SUSPENDED_VM)
        assert multipass("suspend", SUSPENDED_VM)
    suspended_layout = legacy_layout(SUSPENDED_VM, include_disks=False)

    with seeded_vm(DELETED_VM):
        assert multipass("stop", DELETED_VM)
    deleted_layout = legacy_layout(DELETED_VM)
    assert multipass("delete", DELETED_VM)

    scenario.record.update(
        {
            "stopped": {
                **source_record(STOPPED_VM, stopped_layout, stopped_identity),
                "base": base,
                "head": head,
                "snapshot_count": snapshot_count(STOPPED_VM),
                "mount": {
                    "source": str(mount_source),
                    "file": str(mount_file),
                    "content": mount_content,
                },
            },
            "running": {
                **source_record(
                    RUNNING_VM, running_layout, running_identity, record_files=False
                ),
                "sentinel": running_sentinel,
            },
            "suspended": {
                **source_record(
                    SUSPENDED_VM,
                    suspended_layout,
                    suspended_identity,
                    record_files=False,
                ),
                "sentinel": suspended_sentinel,
            },
            "deleted": source_record(DELETED_VM, deleted_layout),
        }
    )


@pytest.mark.verify
@pytest.mark.snapshot
@pytest.mark.scenario(SCENARIO)
def test_hyperv_migration_verify(scenario, multipassd_session_scoped):
    record = scenario.record
    assert current_driver() == "hyperv"
    assert state(STOPPED_VM) == "Stopped"
    assert state(RUNNING_VM) == "Stopped"
    assert state(SUSPENDED_VM) == "Suspended"
    assert state(DELETED_VM) == "Deleted"
    for name in VM_NAMES:
        assert legacy_id_exists(record[RECORD_KEY[name]]["legacy_id"])

    # Running state is intentionally established after the upgrade. A standalone
    # daemon saves running VMs while it shuts down between seed and verify.
    assert multipass("start", RUNNING_VM)
    assert state(RUNNING_VM) == "Running"
    assert_sentinel(RUNNING_VM, record["running"]["sentinel"])
    assert_identity(RUNNING_VM, record["running"]["identity"])
    pre_first_vm_records = legacy_vm_records()
    pre_first_image_records = legacy_image_records()

    # One explicit switch performs the primary bulk migration.
    first_migration = switch_driver("hyperv_api", multipassd_session_scoped)
    assert_first_migration_output(first_migration)

    assert vm_exists(STOPPED_VM)
    assert not vm_exists(RUNNING_VM)
    assert not vm_exists(SUSPENDED_VM)
    assert not vm_exists(DELETED_VM)
    for key in ("stopped", "running", "suspended", "deleted"):
        assert legacy_id_exists(record[key]["legacy_id"])
    current_vm_records = legacy_vm_records()
    current_image_records = legacy_image_records()
    for name in VM_NAMES:
        assert current_vm_records[name] == pre_first_vm_records[name]
        assert current_image_records[name] == pre_first_image_records[name]

    assert_file_records_unchanged(record["stopped"]["disks"])
    assert_file_records_unchanged(record["stopped"]["metadata"])
    assert_file_records_unchanged(record["deleted"]["disks"])
    assert_file_records_unchanged(record["deleted"]["metadata"])
    assert_target_local(STOPPED_VM, record["stopped"])
    assert_target_records(
        STOPPED_VM,
        pre_first_vm_records[STOPPED_VM],
        pre_first_image_records[STOPPED_VM],
    )

    # The migrated target retains guest identity and complete snapshot behavior.
    assert multipass("start", STOPPED_VM, timeout=900)
    assert_sentinel(STOPPED_VM, record["stopped"]["base"])
    assert_sentinel(STOPPED_VM, record["stopped"]["head"])
    assert_identity(STOPPED_VM, record["stopped"]["identity"])
    assert (
        read_file(STOPPED_VM, f"{MOUNT_TARGET}/payload.txt").strip()
        == record["stopped"]["mount"]["content"]
    )

    assert multipass("stop", STOPPED_VM)
    take_snapshot(STOPPED_VM, POST)
    assert snapshot_count(STOPPED_VM) == record["stopped"]["snapshot_count"] + 1

    assert multipass("restore", f"{STOPPED_VM}.{BASE}", "--destructive")
    assert multipass("start", STOPPED_VM)
    assert_sentinel(STOPPED_VM, record["stopped"]["base"])
    assert not path_exists(STOPPED_VM, record["stopped"]["head"]["path"])

    assert multipass("stop", STOPPED_VM)
    assert multipass("restore", f"{STOPPED_VM}.{HEAD}", "--destructive")
    assert multipass("start", STOPPED_VM)
    assert_sentinel(STOPPED_VM, record["stopped"]["head"])
    assert multipass("stop", STOPPED_VM)

    # Switching back reveals the untouched originals. Stop the skipped instances and retry.
    assert switch_driver("hyperv", multipassd_session_scoped)
    assert state(STOPPED_VM) == "Stopped"
    assert multipass("start", STOPPED_VM)
    assert_sentinel(STOPPED_VM, record["stopped"]["base"])
    assert_sentinel(STOPPED_VM, record["stopped"]["head"])
    assert_identity(STOPPED_VM, record["stopped"]["identity"])
    assert (
        read_file(STOPPED_VM, f"{MOUNT_TARGET}/payload.txt").strip()
        == record["stopped"]["mount"]["content"]
    )
    assert multipass("stop", STOPPED_VM)

    assert_sentinel(RUNNING_VM, record["running"]["sentinel"])
    assert_identity(RUNNING_VM, record["running"]["identity"])
    assert multipass("stop", RUNNING_VM)

    assert state(SUSPENDED_VM) == "Suspended"
    assert multipass("start", SUSPENDED_VM)
    assert_sentinel(SUSPENDED_VM, record["suspended"]["sentinel"])
    assert multipass("stop", SUSPENDED_VM)
    assert state(DELETED_VM) == "Deleted"

    pre_retry_disks = {
        name: [file_record(path) for path in legacy_layout(name)["disks"]]
        for name in (STOPPED_VM, RUNNING_VM, SUSPENDED_VM)
    }
    pre_retry_vm_records = legacy_vm_records()
    pre_retry_image_records = legacy_image_records()

    retry_migration = switch_driver("hyperv_api", multipassd_session_scoped)
    assert f"Cannot migrate {STOPPED_VM}: name already taken" in retry_migration.content
    assert RUNNING_VM in retry_migration.content
    assert SUSPENDED_VM in retry_migration.content
    assert f"Cannot migrate {DELETED_VM}: instance is deleted" in retry_migration.content

    for records in pre_retry_disks.values():
        assert_file_records_unchanged(records)
    for name in (RUNNING_VM, SUSPENDED_VM):
        assert vm_exists(name)
        key = RECORD_KEY[name]
        assert legacy_id_exists(record[key]["legacy_id"])
        assert_target_local(name, {"disks": pre_retry_disks[name]})
        assert_target_records(
            name, pre_retry_vm_records[name], pre_retry_image_records[name]
        )
        assert multipass("start", name, timeout=900)
        assert_identity(name, record[key]["identity"])
        assert_sentinel(name, record[key]["sentinel"])
        assert multipass("stop", name)

    # Purging one HCS target leaves its source intact and allows explicit re-migration.
    assert multipass("delete", STOPPED_VM, "--purge")
    assert not vm_exists(STOPPED_VM)
    assert legacy_id_exists(record["stopped"]["legacy_id"])

    assert switch_driver("hyperv", multipassd_session_scoped)
    remigration = switch_driver("hyperv_api", multipassd_session_scoped)
    assert STOPPED_VM in remigration.content
    assert vm_exists(STOPPED_VM)
    assert legacy_id_exists(record["stopped"]["legacy_id"])

    # Purging the original is strictly scoped to the legacy backend.
    assert switch_driver("hyperv", multipassd_session_scoped)
    assert multipass("delete", STOPPED_VM, "--purge")
    assert not legacy_id_exists(record["stopped"]["legacy_id"])
    assert switch_driver("hyperv_api", multipassd_session_scoped)
    assert vm_exists(STOPPED_VM)

    # Leave the upgrade host clean and on the requested legacy test driver.
    assert multipass(
        "delete", STOPPED_VM, RUNNING_VM, SUSPENDED_VM, "--purge", timeout=300
    )
    assert switch_driver("hyperv", multipassd_session_scoped)
    assert multipass(
        "delete", RUNNING_VM, SUSPENDED_VM, DELETED_VM,
        "--purge",
        timeout=300,
    )
    Path(record["stopped"]["mount"]["file"]).unlink()
    Path(record["stopped"]["mount"]["source"]).rmdir()
