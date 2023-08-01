#!/usr/bin/env python3

import itertools
from pathlib import Path
import pprint
import re
import shutil
import subprocess
import sys
import tempfile

from lxml import etree

ARCH_RE=re.compile(r"\.(x86_64|aarch64)")

OS_VERSION_XPATH="//allowed-os-versions/os-version"

x86_src, arm_src, dest = (Path(arg) for arg in sys.argv[1:4])

with tempfile.TemporaryDirectory() as workdir:
    workdir = Path(workdir)
    x86_work, arm_work, dest_work = (workdir / path.name for path in (x86_src, arm_src, dest))

    subprocess.check_call(["pkgutil", "--expand", x86_src, x86_work])
    subprocess.check_call(["pkgutil", "--expand", arm_src, arm_work])

    def target(parent, path):
        return dest_work / ARCH_RE.sub("", str(path.relative_to(parent)))

    def copy(parent, path):
        target_path = target(parent, path)
        target_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(path, target_path)

    for path in x86_work.rglob("*"):
        # Copy metadata
        if path.is_dir() or path.name in ("Bom", "Payload"):
            continue
        copy(x86_work, path)

    for payload in itertools.chain(*(work.glob("*.pkg/Payload") for work in (x86_work, arm_work))):
        # Unpack payload
        unpacked = payload.with_suffix(".unpacked")
        unpacked.mkdir()
        # need to use system tar for deprecated cpio support
        subprocess.check_call(["/usr/bin/tar", "-xzf", payload, "-C", unpacked])

    for path in x86_work.glob("*.pkg/Payload.unpacked/**/*"):
        if path.is_dir():
            continue

        target_path = target(x86_work, path)

        if path.name != "Info.plist" and ({"bin", "lib"} & set(path.parts)):
            arm_path = arm_work / ARCH_RE.sub(".aarch64", str(path.relative_to(x86_work)))
            if arm_path.exists():
                target_path.parent.mkdir(parents=True, exist_ok=True)
                # If it's a binary, exists on arm and it is not universal, make it a universal one
                binary_archs = subprocess.run(["lipo", "-archs", arm_path], capture_output=True).stdout.decode().rstrip()
                if binary_archs == "arm64":
                    subprocess.check_call(["lipo", "-create", "-output", target_path, path, arm_path])
                else:
                    subprocess.check_call(["cp", arm_path, target_path])
                continue

        # Copy all other paths over
        copy(x86_work, path)

    for path in arm_work.glob("*.pkg/Payload.unpacked/**/*"):
        # Copy all remaining ARM-specific paths
        target_path = target(arm_work, path)

        if path.is_dir() or target_path.exists():
            continue

        copy(arm_work, path)

    # Fix the Distribution file
    dist_path = str(dest_work / "Distribution")
    dist = etree.parse(dist_path)
    x86_dist = etree.parse(str(x86_work / "Distribution"))
    arm_dist = etree.parse(str(arm_work / "Distribution"))

    # Set the host architectures
    dist.find("options").attrib["hostArchitectures"] = ",".join(
        set(v for v in itertools.chain(*(
            d.find("options").attrib["hostArchitectures"].split(",") for d in (x86_dist, arm_dist)))
        )
    )
    version = ARCH_RE.sub("", dist.find("product").attrib["version"])
    dist.find("product").attrib["version"] = version

    for pkg in dist.findall("pkg-ref"):
        if "version" in pkg.attrib:
            pkg.attrib["version"] = version

        if pkg.text:
            pkg.text = ARCH_RE.sub("", pkg.text)

    # Set the minimum OSX version
    dist.xpath(OS_VERSION_XPATH)[0].attrib["min"] = (
        min(x86_dist.xpath(OS_VERSION_XPATH)[0].attrib["min"], arm_dist.xpath(OS_VERSION_XPATH)[0].attrib["min"])
    )

    dist.write(str(dist_path), pretty_print=True, xml_declaration=True)

    # Recreate the package
    pkgs = dest_work / "Packages"
    pkgs.mkdir()
    for component in ("multipass", "multipassd", "multipass_gui"):
        pkg = dest_work / f"multipass-{version}-Darwin-{component}.pkg"
        subprocess.check_call(["pkgbuild", "--root", pkg / "Payload.unpacked",
                               "--identifier", f"com.canonical.multipass.{component}",
                               "--scripts", pkg / "Scripts",
                               "--version", version,
                               "--install-location", "/",
                               pkgs / pkg.name])

    subprocess.check_call(["productbuild", "--distribution", dist_path,
                           "--package-path", pkgs,
                           "--resources", dest_work / "Resources",
                           "--version", version,
                           dest])
