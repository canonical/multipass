#!/usr/bin/env python3
# Copyright (C) Canonical, Ltd.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

"""Resolve the pinned MinGW-w64 package closure needed to build qemu-img on
Windows and emit a lockfile of `URL SHA512` pairs.

The lockfile is consumed by portfile.cmake, which feeds the pairs to
`vcpkg_acquire_msys(... DIRECT_PACKAGES ...)`. `DIRECT_PACKAGES` entries carry
no dependency metadata, so the *entire* transitive closure has to be pinned
here explicitly. This script resolves that closure directly from the MSYS2
pacman sync databases (the same data `pacman` itself uses), then downloads each
package to compute its SHA-512 (the databases only publish SHA-256, but
vcpkg_acquire_msys requires SHA-512).

Run it whenever the toolchain is bumped:

    python generate-mingw-lockfile.py

It rewrites `mingw64-toolchain.lock` in place. No MSYS2 installation is needed.
"""

from __future__ import annotations

import argparse
import hashlib
import sys
import tarfile
import urllib.request
from pathlib import Path

import zstandard

# Root packages we explicitly want; their transitive closure is resolved below.
# Mirrors the set the Windows QEMU build has always installed: the GCC
# toolchain, GLib (qemu-img links it), zstd (qcow2 zstd compression), and the
# Python/Ninja/pkgconf build tools driving the meson build.
ROOT_PACKAGES = [
    "mingw-w64-x86_64-gcc",
    "mingw-w64-x86_64-glib2",
    "mingw-w64-x86_64-zstd",
    "mingw-w64-x86_64-python",
    "mingw-w64-x86_64-ninja",
    "mingw-w64-x86_64-pkgconf",
]

# The mingw64 repository is self-contained (mingw packages depend only on other
# mingw packages); the msys shell layer (bash, coreutils, make, ...) is provided
# separately by vcpkg_acquire_msys's own maintained package set.
REPO_BASE = "https://repo.msys2.org/mingw/mingw64"
DB_URL = f"{REPO_BASE}/mingw64.db"

LOCKFILE = Path(__file__).with_name("mingw64-toolchain.lock")


def strip_constraint(dep: str) -> str:
    """Drop any version constraint (`>=`, `=`, `<`, ...) from a dependency."""
    return re.sub("(>|<|=).*", "", dep)


def _urlopen(url: str):
    req = urllib.request.Request(url, headers={"User-Agent": "multipass-lockgen"})
    return urllib.request.urlopen(req)


def fetch(url: str) -> bytes:
    with _urlopen(url) as resp:
        return resp.read()


def parse_db(db_bytes: bytes) -> dict[str, dict]:
    """Parse a pacman sync database (a zstd-compressed tar of `desc` files)."""
    packages: dict[str, dict] = {}
    with zstandard.ZstdDecompressor().stream_reader(db_bytes) as zstd, \
            tarfile.open(fileobj=zstd, mode="r|") as tar:
        for member in tar:
            if not member.name.endswith("/desc"):
                continue
            desc = tar.extractfile(member).read().decode("utf-8")
            fields: dict[str, list[str]] = {}
            key = None
            for line in desc.splitlines():
                if line.startswith("%") and line.endswith("%"):
                    key = line.strip("%")
                    fields[key] = []
                elif line.strip() and key is not None:
                    fields[key].append(line.strip())
            name = fields["NAME"][0]
            packages[name] = {
                "filename": fields["FILENAME"][0],
                "depends": [strip_constraint(d) for d in fields.get("DEPENDS", [])],
                "provides": [strip_constraint(p) for p in fields.get("PROVIDES", [])],
            }
    return packages


def resolve_closure(packages: dict[str, dict], roots: list[str]) -> list[str]:
    provides = {}
    for name, meta in packages.items():
        for prov in meta["provides"]:
            provides.setdefault(prov, name)

    def resolve_name(dep: str) -> str | None:
        if dep in packages:
            return dep
        return provides.get(dep)

    closure: set[str] = set()
    backlog = list(roots)
    while backlog:
        dep = backlog.pop()
        name = resolve_name(dep)
        if name is None:
            raise SystemExit(f"error: could not resolve dependency '{dep}'")
        if name in closure:
            continue
        closure.add(name)
        backlog.extend(packages[name]["depends"])
    return sorted(closure)


def sha512_of(url: str) -> str:
    digest = hashlib.sha512()
    with _urlopen(url) as resp:
        for chunk in iter(lambda: resp.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Only resolve and print the closure; do not download/hash or write the lockfile.",
    )
    args = parser.parse_args()

    print(f"Fetching {DB_URL} ...", file=sys.stderr)
    packages = parse_db(fetch(DB_URL))
    print(f"  {len(packages)} packages in mingw64 database", file=sys.stderr)

    closure = resolve_closure(packages, ROOT_PACKAGES)
    print(f"Resolved closure: {len(closure)} packages", file=sys.stderr)
    for name in closure:
        print(f"  {name}", file=sys.stderr)

    if args.dry_run:
        return 0

    lines = []
    for i, name in enumerate(closure, 1):
        url = f"{REPO_BASE}/{packages[name]['filename']}"
        print(f"[{i}/{len(closure)}] hashing {name} ...", file=sys.stderr)
        lines.append(f"{url} {sha512_of(url)}")

    header = (
        "# Pinned MinGW-w64 package closure for the Windows qemu-img build.\n"
        "# Generated by generate-mingw-lockfile.py -- do not edit by hand.\n"
        "# Format: <url> <sha512>. Consumed by portfile.cmake via\n"
        "# vcpkg_acquire_msys(... DIRECT_PACKAGES ...).\n"
    )
    LOCKFILE.write_text(header + "\n".join(lines) + "\n", encoding="utf-8")
    print(f"Wrote {LOCKFILE} ({len(lines)} packages)", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
