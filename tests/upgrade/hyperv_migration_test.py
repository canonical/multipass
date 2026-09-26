#!/usr/bin/env python3

# TODO hyperv migration, remove (whole file)

import hashlib
import ipaddress
import json
import logging
import os
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

import pytest

from cli.config import cfg
from cli.multipass import (
    exec,
    get_cloudinit_instance_id,
    get_default_interface_name,
    get_mac_addr_of,
    info,
    launch,
    multipass,
    path_exists,
    read_file,
    snapshot_count,
    state,
    vm_exists,
    write_file,
)
from cli.utilities import sudo, uuid4_str

STOPPED_VM = "upg-hcs-phase1-stopped"
RUNNING_VM = "upg-hcs-phase1-running"
SUSPENDED_VM = "upg-hcs-phase1-suspended"
DELETED_VM = "upg-hcs-phase1-deleted"
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

# Networking scenario: extra interfaces on vSwitches created through Hyper-V, not by Multipass.
NETWORK_VM = "upg-hcs-phase1-network"
PRIVATE_SWITCH = "mpupg-hcs-private"
PRIVATE_MAC = "52:54:00:4d:50:11"
# An external switch rebinds a physical adapter, so it's only covered when an adapter that
# doesn't carry host traffic is given explicitly.
EXTERNAL_ADAPTER_ENV = "MP_UPGRADE_EXTERNAL_ADAPTER"
EXTERNAL_SWITCH = "mpupg-hcs-external"
EXTERNAL_MAC = "52:54:00:4d:50:12"
DEFAULT_SWITCH_HOST_VNIC = "vEthernet (Default Switch)"

pytestmark = pytest.mark.skipif(
    sys.platform != "win32" or getattr(cfg, "driver", None) != "hyperv",
    reason="Hyper-V migration verification requires Windows and the hyperv driver",
)


def seeded_vm(name):
    if vm_exists(name):
        assert multipass("delete", name, "--purge")
    return launch(cfg_override={"name": name, "autopurge": False})


def powershell(script):
    return subprocess.run(
        [
            "powershell.exe",
            "-NoProfile",
            "-NonInteractive",
            "-Command",
            script,
        ],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()


def ps_quote(value):
    return "'" + str(value).replace("'", "''") + "'"


def disk_chain(path):
    output = powershell(f"""
$current = {ps_quote(path)}
$chain = @()
while ($current) {{
    $chain += $current
    $current = (Get-VHD -Path $current -ErrorAction Stop).ParentPath
}}
ConvertTo-Json -InputObject $chain -Compress
""")
    return json.loads(output)


def disk_paths(*roots):
    disks = {}
    for root in roots:
        for disk in disk_chain(root):
            key = os.path.normcase(os.path.abspath(disk))
            disks.setdefault(key, Path(disk))
    return list(disks.values())


def legacy_layout(name, include_disks=True):
    output = powershell(f"""
$vm = Get-VM -Name {ps_quote(name)} -ErrorAction Stop
$primary = @(Get-VMHardDiskDrive -VMName $vm.Name |
    Where-Object {{ $_.ControllerType -eq 'SCSI' -and
                    $_.ControllerNumber -eq 0 -and
                    $_.ControllerLocation -eq 0 }})
if ($primary.Count -ne 1) {{ throw 'Expected one primary disk' }}
$snapshot_disks = @(Get-VMCheckpoint -VMName $vm.Name | ForEach-Object {{
    $disk = @(Get-VMHardDiskDrive -VMSnapshot $_ |
        Where-Object {{ $_.ControllerType -eq 'SCSI' -and
                        $_.ControllerNumber -eq 0 -and
                        $_.ControllerLocation -eq 0 }})
    if ($disk.Count -ne 1) {{ throw 'Expected one checkpoint disk' }}
    $disk[0].Path
}})
[PSCustomObject]@{{
    id = $vm.Id.ToString()
    disks = @($primary[0].Path) + $snapshot_disks
}} | ConvertTo-Json -Compress -Depth 4
""")
    result = json.loads(output)
    result["disks"] = disk_paths(*result["disks"]) if include_disks else []
    return result


def legacy_id_exists(vm_id):
    return (
        powershell(
            f"[bool](Get-VM -Id {ps_quote(vm_id)} -ErrorAction SilentlyContinue)"
        )
        == "True"
    )


def file_record(path):
    path = Path(path)
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(4 * 1024 * 1024), b""):
            digest.update(chunk)
    return {
        "path": str(path),
        "size": path.stat().st_size,
        "sha256": digest.hexdigest(),
    }


def assert_file_records_unchanged(records):
    for record in records:
        assert Path(record["path"]).exists()
        assert file_record(record["path"]) == record


def backend_dir(target=False):
    return Path(cfg.data_dir) / ("hcs" if target else "")


def instance_dir(name, target=False):
    return backend_dir(target) / "vault" / "instances" / name


def database(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def vm_records(target=False):
    return database(backend_dir(target) / "multipassd-vm-instances.json")


def image_records(target=False):
    return database(
        backend_dir(target)
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
        "ssh_host_key": read_file(
            name, "/etc/ssh/ssh_host_ed25519_key.pub"
        ).strip(),
        "cloud_init_id": get_cloudinit_instance_id(name).strip(),
        "mac": get_mac_addr_of(name, interface).strip().lower(),
    }


def take_snapshot(name, snapshot_name):
    assert state(name) == "Stopped"
    assert multipass("snapshot", name, "--name", snapshot_name)


def source_record(name, layout, guest_identity=None):
    directory = instance_dir(name)
    metadata_files = [
        path
        for path in (
            directory / "cloud-init-config.iso",
            directory / "snapshot-head",
            directory / "snapshot-count",
            *sorted(directory.glob("*.snapshot.json")),
        )
        if path.exists()
    ]
    return {
        "legacy_id": layout["id"],
        "disks": [file_record(path) for path in layout["disks"]],
        "metadata": [file_record(path) for path in metadata_files],
        "identity": guest_identity,
    }


def assert_target_local(name, source_disks):
    directory = instance_dir(name, target=True)
    active_disk = Path(image_records(target=True)[name]["image"]["path"])
    assert path_is_within(active_disk, directory)
    snapshot_disks = [
        directory / f"{database(path)['snapshot']['index']}.avhdx"
        for path in directory.glob("*.snapshot.json")
    ]
    target_disks = disk_paths(active_disk, *snapshot_disks)
    assert len(target_disks) == len(source_disks)
    assert sorted(path.stat().st_size for path in target_disks) == sorted(
        record["size"] for record in source_disks
    )
    for path in target_disks:
        assert path_is_within(path, directory)

    assert (directory / "cloud-init-config.iso").exists()
    assert not (directory / "migration-transaction.json").exists()


def assert_target_records(name, expected_vm, expected_image):
    assert vm_records(target=True)[name] == expected_vm
    actual_image = image_records(target=True)[name]
    assert path_is_within(
        actual_image["image"]["path"], instance_dir(name, target=True)
    )
    actual_image["image"]["path"] = expected_image["image"]["path"]
    assert actual_image == expected_image


def assert_output(output, *messages):
    for message in messages:
        assert message in output.content


def migrated_names(output):
    """The instances listed as successfully migrated in a driver switch's output."""
    lines = output.content.splitlines()
    for index, line in enumerate(lines):
        if "The following instances were successfully migrated" in line:
            names = []
            for entry in lines[index + 1 :]:
                if not entry.startswith("  "):
                    break
                names.append(entry.strip())
            return set(names)
    return set()


def create_switch(name, *switch_args):
    """Create a vSwitch through Hyper-V, replacing any leftover from an interrupted run."""
    remove_switch(name)
    logging.info("hyperv-migration :: creating vSwitch `%s`", name)
    subprocess.run(
        sudo(
            "powershell",
            "-NoProfile",
            "-NonInteractive",
            "-Command",
            f"New-VMSwitch -Name {ps_quote(name)} {' '.join(switch_args)} | Out-Null",
        ),
        check=True,
    )
    return name


def remove_switch(name):
    subprocess.run(
        sudo(
            "powershell",
            "-NoProfile",
            "-NonInteractive",
            "-Command",
            f"Remove-VMSwitch -Name {ps_quote(name)} -Force -ErrorAction SilentlyContinue",
        ),
        check=False,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def launch_with_networks(name, networks):
    """Launch `name` with a manual-mode extra interface per (switch, mac) in `networks`."""
    network_args = [
        arg
        for switch, mac in networks
        for arg in ("--network", f"name={switch},mode=manual,mac={mac}")
    ]
    result = multipass(
        "launch",
        "--cpus",
        cfg.vm.cpus,
        "--memory",
        cfg.vm.memory,
        "--disk",
        cfg.vm.disk,
        "--name",
        name,
        *network_args,
        "--timeout",
        getattr(cfg.timeouts, "launch", 300),
        cfg.vm.image,
        retry=getattr(cfg.retries, "launch", 0),
    )
    assert result, f"Failed to launch `{name}`: {result}"


def guest_macs(name):
    """Sorted MAC addresses of the guest's interfaces, loopback excluded."""
    with exec(name, "sh", "-c", "cat /sys/class/net/*/address") as result:
        assert result, f"Could not read `{name}`'s interfaces: {result}"
        return sorted(
            mac.lower() for mac in str(result).split() if mac != "00:00:00:00:00:00"
        )


def management_ip(name):
    return ipaddress.ip_address(info(name)[name]["ipv4"][0])


def host_subnet(interface_alias):
    address = powershell(
        f"Get-NetIPAddress -AddressFamily IPv4 -InterfaceAlias {ps_quote(interface_alias)} "
        "-ErrorAction Stop | Select-Object -First 1 | "
        'ForEach-Object { "$($_.IPAddress)/$($_.PrefixLength)" }'
    )
    return ipaddress.ip_interface(address).network


def assert_stopped_guest(source):
    assert_sentinel(STOPPED_VM, source["base"])
    assert_sentinel(STOPPED_VM, source["head"])
    assert identity(STOPPED_VM) == source["identity"]
    assert (
        read_file(STOPPED_VM, f"{MOUNT_TARGET}/payload.txt").strip()
        == source["mount"]["content"]
    )


@pytest.mark.seed
@pytest.mark.snapshot
@pytest.mark.scenario(STOPPED_VM)
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
        assert (
            read_file(STOPPED_VM, f"{MOUNT_TARGET}/payload.txt").strip()
            == mount_content
        )
        assert multipass("stop", STOPPED_VM)
        take_snapshot(STOPPED_VM, BASE)

        assert multipass("start", STOPPED_VM)
        head = sentinel(STOPPED_VM, "hv-head")
        assert multipass("stop", STOPPED_VM)
        take_snapshot(STOPPED_VM, HEAD)

    record = scenario.record
    record["stopped"] = {
        **source_record(
            STOPPED_VM, legacy_layout(STOPPED_VM), stopped_identity
        ),
        "base": base,
        "head": head,
        "snapshot_count": snapshot_count(STOPPED_VM),
        "mount": {
            "source": str(mount_source),
            "file": str(mount_file),
            "content": mount_content,
        },
    }
    for name, command in ((RUNNING_VM, "stop"), (SUSPENDED_VM, "suspend")):
        key = RECORD_KEY[name]
        with seeded_vm(name):
            guest_sentinel = sentinel(name, f"hv-{key}")
            guest_identity = identity(name)
            assert multipass(command, name)
        record[key] = {
            "legacy_id": legacy_layout(name, include_disks=False)["id"],
            "identity": guest_identity,
            "sentinel": guest_sentinel,
        }

    with seeded_vm(DELETED_VM):
        assert multipass("stop", DELETED_VM)
    deleted_layout = legacy_layout(DELETED_VM)
    assert multipass("delete", DELETED_VM)
    record["deleted"] = source_record(DELETED_VM, deleted_layout)


@pytest.mark.seed
@pytest.mark.scenario(NETWORK_VM)
def test_hyperv_migration_network_seed(scenario):
    assert current_driver() == "hyperv"

    networks = [(create_switch(PRIVATE_SWITCH, "-SwitchType", "Private"), PRIVATE_MAC)]
    if adapter := os.environ.get(EXTERNAL_ADAPTER_ENV):
        networks.append(
            (
                create_switch(
                    EXTERNAL_SWITCH,
                    "-NetAdapterName",
                    ps_quote(adapter),
                    "-AllowManagementOS",
                    "$true",
                ),
                EXTERNAL_MAC,
            )
        )

    if vm_exists(NETWORK_VM):
        assert multipass("delete", NETWORK_VM, "--purge")
    launch_with_networks(NETWORK_VM, networks)

    macs = guest_macs(NETWORK_VM)
    for _, mac in networks:
        assert mac in macs, f"extra interface {mac} missing from the guest: {macs}"

    record = scenario.record
    record["networks"] = [{"switch": switch, "mac": mac} for switch, mac in networks]
    record["macs"] = macs
    record["identity"] = identity(NETWORK_VM)
    assert multipass("stop", NETWORK_VM)


@dataclass
class Migration:
    """The outcome of the one driver switch every verify test builds on."""

    output: object
    # The main scenario's record, and the network scenario's if it was seeded.
    record: dict
    network: dict | None
    # The legacy records as they were right before the switch.
    vm_records: dict
    image_records: dict


def purge_all(names):
    for name in names:
        if vm_exists(name):
            multipass("delete", name, "--purge", timeout=300)


def clean_up(record, network, governor):
    """Best effort: purge every scenario instance from both drivers and remove the host
    resources. Only switches towards hyperv, since switching to hcs would migrate."""
    names = [*RECORD_KEY, *([NETWORK_VM] if network else [])]
    try:
        if current_driver() == "hcs":
            purge_all(names)
            switch_driver("hyperv", governor)
        purge_all(names)
    finally:
        for switch in (network or {}).get("networks", []):
            remove_switch(switch["switch"])
        mount = record["stopped"]["mount"]
        Path(mount["file"]).unlink(missing_ok=True)
        if Path(mount["source"]).exists():
            Path(mount["source"]).rmdir()


@pytest.fixture(scope="module")
def migration(verify_manifest, multipassd_session_scoped):
    """The upgrade's single driver switch, shared by every verify test.

    A real upgrade switches once and every eligible instance migrates together, so the
    scenarios are verified against that one migration instead of each switching on its own
    (which let one scenario's switch migrate another's instance).
    """
    if STOPPED_VM not in verify_manifest:
        pytest.skip(f"Scenario `{STOPPED_VM}` was not seeded; nothing to verify.")
    record = verify_manifest[STOPPED_VM]
    network = verify_manifest.get(NETWORK_VM)

    try:
        assert current_driver() == "hyperv"
        assert state(STOPPED_VM) == "Stopped"
        assert state(RUNNING_VM) == "Stopped"
        assert state(SUSPENDED_VM) == "Suspended"
        assert state(DELETED_VM) == "Deleted"
        for key in RECORD_KEY.values():
            assert legacy_id_exists(record[key]["legacy_id"])
        if network:
            assert state(NETWORK_VM) == "Stopped"

        # Running state is intentionally established after the upgrade. A standalone
        # daemon saves running VMs while it shuts down between seed and verify.
        assert multipass("start", RUNNING_VM)
        assert state(RUNNING_VM) == "Running"
        assert_sentinel(RUNNING_VM, record["running"]["sentinel"])
        assert identity(RUNNING_VM) == record["running"]["identity"]

        pre_vm_records = vm_records()
        pre_image_records = image_records()
        output = switch_driver("hcs", multipassd_session_scoped)

        yield Migration(output, record, network, pre_vm_records, pre_image_records)
    finally:
        clean_up(record, network, multipassd_session_scoped)


@pytest.mark.verify
def test_hyperv_migration_verify_output(migration):
    """Exactly the eligible instances migrate; the others are refused with a reason."""
    expected = {STOPPED_VM, *([NETWORK_VM] if migration.network else [])}
    assert migrated_names(migration.output) == expected
    assert_output(
        migration.output,
        f"Cannot migrate {RUNNING_VM}: Hyper-V reports state 'Running'",
        f"Cannot migrate {SUSPENDED_VM}: Hyper-V reports state 'Saved'",
        f"Cannot migrate {DELETED_VM}: instance is deleted",
        "Do not run an original and its hcs copy at the same time",
    )


@pytest.mark.verify
def test_hyperv_migration_verify_refused(migration):
    """Refused instances are left untouched on the legacy side."""
    record = migration.record
    current_vm_records = vm_records()
    current_image_records = image_records()
    for name in (RUNNING_VM, SUSPENDED_VM, DELETED_VM):
        assert not vm_exists(name)
        assert legacy_id_exists(record[RECORD_KEY[name]]["legacy_id"])
        assert current_vm_records[name] == migration.vm_records[name]
        assert current_image_records[name] == migration.image_records[name]

    assert_file_records_unchanged(record["deleted"]["disks"] + record["deleted"]["metadata"])


@pytest.mark.verify
@pytest.mark.snapshot
def test_hyperv_migration_verify_stopped(migration):
    """The stopped instance migrates with its disks, records, identity and snapshots."""
    record = migration.record["stopped"]
    assert vm_exists(STOPPED_VM)
    assert legacy_id_exists(record["legacy_id"])
    assert vm_records()[STOPPED_VM] == migration.vm_records[STOPPED_VM]
    assert image_records()[STOPPED_VM] == migration.image_records[STOPPED_VM]

    assert_file_records_unchanged(record["disks"] + record["metadata"])
    assert_target_local(STOPPED_VM, record["disks"])
    assert_target_records(
        STOPPED_VM,
        migration.vm_records[STOPPED_VM],
        migration.image_records[STOPPED_VM],
    )

    # The migrated target retains guest identity and complete snapshot behavior.
    assert multipass("start", STOPPED_VM, timeout=900)
    assert_stopped_guest(record)

    assert multipass("stop", STOPPED_VM)
    take_snapshot(STOPPED_VM, POST)
    assert snapshot_count(STOPPED_VM) == record["snapshot_count"] + 1

    for snapshot, key in ((BASE, "base"), (HEAD, "head")):
        assert multipass("restore", f"{STOPPED_VM}.{snapshot}", "--destructive")
        assert multipass("start", STOPPED_VM)
        assert_sentinel(STOPPED_VM, record[key])
        if snapshot == BASE:
            assert not path_exists(STOPPED_VM, record["head"]["path"])
        assert multipass("stop", STOPPED_VM)


@pytest.mark.verify
def test_hyperv_migration_verify_network(migration):
    """The network instance migrates with its extra interfaces on vSwitches Multipass
    didn't create."""
    network = migration.network
    if network is None:
        pytest.skip(f"Scenario `{NETWORK_VM}` was not seeded; nothing to verify.")

    # The extra interfaces stay on their vSwitches (an external switch's adapter can't be
    # bridged again) and keep their MACs.
    assert vm_exists(NETWORK_VM)
    target_interfaces = vm_records(target=True)[NETWORK_VM]["extra_interfaces"]
    assert [(i["id"], i["mac_address"]) for i in target_interfaces] == [
        (entry["switch"], entry["mac"]) for entry in network["networks"]
    ]

    assert multipass("start", NETWORK_VM, timeout=900)
    assert guest_macs(NETWORK_VM) == network["macs"]
    assert identity(NETWORK_VM) == network["identity"]
    assert multipass("stop", NETWORK_VM)


@pytest.mark.verify
def test_hyperv_migration_verify_follow_up(migration, multipassd_session_scoped):
    """What needs further driver switches, on top of the one migration. Runs last, as it
    moves the instances the other verify tests check."""
    record = migration.record
    network = migration.network
    governor = multipassd_session_scoped

    # Switching back reveals the untouched originals. Stop the skipped instances and retry.
    switch_driver("hyperv", governor)
    assert state(STOPPED_VM) == "Stopped"
    assert multipass("start", STOPPED_VM)
    assert_stopped_guest(record["stopped"])
    assert multipass("stop", STOPPED_VM)

    assert_sentinel(RUNNING_VM, record["running"]["sentinel"])
    assert identity(RUNNING_VM) == record["running"]["identity"]
    assert multipass("stop", RUNNING_VM)

    assert state(SUSPENDED_VM) == "Suspended"
    assert multipass("start", SUSPENDED_VM)
    assert_sentinel(SUSPENDED_VM, record["suspended"]["sentinel"])
    assert multipass("stop", SUSPENDED_VM)
    assert state(DELETED_VM) == "Deleted"

    if network:
        # The migration created the AZ networks, which share the Default Switch's vSwitch and
        # also hand out leases to its instances. The retained original must still resolve its
        # management IP on the Default Switch, rather than a stray AZ lease for its MAC.
        assert multipass("start", NETWORK_VM, timeout=900)
        assert management_ip(NETWORK_VM) in host_subnet(DEFAULT_SWITCH_HOST_VNIC)
        assert identity(NETWORK_VM) == network["identity"]
        assert multipass("stop", NETWORK_VM)

    pre_retry_disks = {
        name: [file_record(path) for path in legacy_layout(name)["disks"]]
        for name in (STOPPED_VM, RUNNING_VM, SUSPENDED_VM)
    }
    pre_retry_vm_records = vm_records()
    pre_retry_image_records = image_records()

    retry = switch_driver("hcs", governor)
    assert migrated_names(retry) == {RUNNING_VM, SUSPENDED_VM}
    assert_output(
        retry,
        f"Cannot migrate {STOPPED_VM}: name already taken",
        f"Cannot migrate {DELETED_VM}: instance is deleted",
        *([f"Cannot migrate {NETWORK_VM}: name already taken"] if network else []),
    )

    for records in pre_retry_disks.values():
        assert_file_records_unchanged(records)
    for name in (RUNNING_VM, SUSPENDED_VM):
        assert vm_exists(name)
        key = RECORD_KEY[name]
        assert legacy_id_exists(record[key]["legacy_id"])
        assert_target_local(name, pre_retry_disks[name])
        assert_target_records(
            name, pre_retry_vm_records[name], pre_retry_image_records[name]
        )
        assert multipass("start", name, timeout=900)
        assert identity(name) == record[key]["identity"]
        assert_sentinel(name, record[key]["sentinel"])
        assert multipass("stop", name)

    # Purging one HCS target leaves its source intact and allows explicit re-migration.
    assert multipass("delete", STOPPED_VM, "--purge")
    assert not vm_exists(STOPPED_VM)
    assert legacy_id_exists(record["stopped"]["legacy_id"])

    switch_driver("hyperv", governor)
    remigration = switch_driver("hcs", governor)
    assert migrated_names(remigration) == {STOPPED_VM}
    assert vm_exists(STOPPED_VM)
    assert legacy_id_exists(record["stopped"]["legacy_id"])

    # Purging the original is strictly scoped to the legacy backend.
    switch_driver("hyperv", governor)
    assert multipass("delete", STOPPED_VM, "--purge")
    assert not legacy_id_exists(record["stopped"]["legacy_id"])
    switch_driver("hcs", governor)
    assert vm_exists(STOPPED_VM)
