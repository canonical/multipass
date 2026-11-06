"""Artifact protection between steps.

Locked artifacts (everything the current step does not own as output) are snapshotted,
hashed and made read-only before the step runs. Afterwards they are verified; any
modification or deletion is reverted from the snapshot and reported so the step can be
rejected. chmod alone is not enough (a shell can chmod back or delete+recreate), hence
the hash check is authoritative.
"""

from __future__ import annotations

import hashlib
import shutil
import stat
import subprocess
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

_WRITE_BITS = stat.S_IWUSR | stat.S_IWGRP | stat.S_IWOTH


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


ABSENT = ""  # digest marking a locked artifact that did not exist at step start (it must stay absent)


def matches(path: Path, digest: str) -> bool:
    if digest == ABSENT:
        return not path.exists() and not path.is_symlink()
    return not path.is_symlink() and path.is_file() and sha256(path) == digest


@dataclass
class LockedSet:
    snapshot_dir: Path
    entries: dict[str, tuple[Path, str, int]] = field(default_factory=dict)  # name -> (path, sha|ABSENT, mode)

    def unlock(self) -> None:
        for path, digest, mode in self.entries.values():
            if digest != ABSENT and path.is_file() and not path.is_symlink():
                _try_chmod(path, mode)

    def verify_and_restore(self) -> list[str]:
        """Return names of tampered artifacts; each is restored from its snapshot (or removed if it was absent)."""
        tampered = []
        for name, (path, digest, mode) in self.entries.items():
            if matches(path, digest):
                continue
            tampered.append(name)
            if path.is_symlink() or path.is_file():
                path.unlink()  # unlink needs a writable dir, not a writable file
            elif path.is_dir():
                shutil.rmtree(path)
            if digest != ABSENT:
                path.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(self.snapshot_dir / name, path)
                _try_chmod(path, mode)
        return tampered


def _try_chmod(path: Path, mode: int) -> None:
    # Best effort: some filesystems (e.g. sshfs without default_permissions) ignore or reject chmod.
    try:
        path.chmod(mode)
    except OSError:
        pass


def lock(artifacts: dict[str, Path], snapshot_dir: Path) -> LockedSet:
    """Snapshot + chmod a-w every existing artifact; record absent ones so their creation is detected."""
    snapshot_dir.mkdir(parents=True, exist_ok=True)
    locked = LockedSet(snapshot_dir)
    for name, path in artifacts.items():
        if path.is_symlink() or not path.is_file():
            locked.entries[name] = (path, ABSENT, 0)
            continue
        mode = stat.S_IMODE(path.stat().st_mode)
        shutil.copy2(path, snapshot_dir / name)
        locked.entries[name] = (path, sha256(path), mode)
        _try_chmod(path, mode & ~_WRITE_BITS)
    return locked


def workspace_fingerprint(workspace: Path) -> Optional["WorkspaceFingerprint"]:
    """Hash of tracked+untracked changes via git, or None when git is unavailable.

    Used to detect read-only roles modifying the source tree. Run artifacts under
    .multiflow/ are excluded.
    """
    try:
        status = subprocess.run(
            ["git", "status", "--porcelain=v1", "-z", "--untracked-files=all", "--", ".", ":(exclude).multiflow"],
            cwd=workspace, capture_output=True, timeout=60, check=True,
        ).stdout
        diff = subprocess.run(
            ["git", "diff", "--binary", "HEAD", "--", ".", ":(exclude).multiflow"],
            cwd=workspace, capture_output=True, timeout=120, check=True,
        ).stdout
    except (OSError, subprocess.SubprocessError):
        return None
    untracked = {}
    for entry in status.split(b"\0"):
        if entry.startswith(b"?? "):
            rel = entry[3:].decode(errors="replace")
            p = workspace / rel
            if p.is_file():
                untracked[rel] = sha256(p)
    return WorkspaceFingerprint(hashlib.sha256(diff).hexdigest(), untracked)


@dataclass
class WorkspaceFingerprint:
    tracked: str  # hash of `git diff HEAD` (all tracked-file changes)
    untracked: dict[str, str]  # path -> sha256

    def compare(self, after: "WorkspaceFingerprint") -> tuple[list[str], list[str]]:
        """Return (modifications, new_files). Modifications: tracked changes or pre-existing untracked
        files changed/deleted (i.e. source edits). New files: untracked files that appeared (often
        build/test by-products)."""
        modified = []
        if after.tracked != self.tracked:
            modified.append("tracked files (see `git diff`)")
        modified += [p for p, d in self.untracked.items() if after.untracked.get(p) != d]
        new = sorted(set(after.untracked) - set(self.untracked))
        return modified, new
