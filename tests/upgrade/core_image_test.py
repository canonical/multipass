#!/usr/bin/env python3
#
# Copyright (C) Canonical, Ltd.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 3.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#

"""An Ubuntu Core VM's daemon-side record and reported identity should survive an
upgrade. Covered for `core24` and `core26` (both stopped); the `core26` image
only exists from 1.16.3, so its seed is gated.

Images are named without a remote (`core24`, not `core:core24`): the seed runs on
the *from* version, where core images still live in the default remote. A later
version may move them under a `core:` remote, but that only affects fresh
launches, not the already-instantiated VMs these scenarios upgrade.

Like the cloud images in `lifecycle_test`, each scenario writes a guest sentinel
before the upgrade and asserts it byte-for-byte after, alongside the
`image_release`/cpu/memory fingerprint and a park/resume round trip. This relies
on `multipass exec` working on Core; `resume_seeded` waits for exec-readiness and
fails clearly if it does not."""

import platform

import pytest

from cli.multipass.feature_versions import skip_if_feature_not_supported
from .helpers import (
    seed_sentinel,
    assert_sentinel_record,
    info_fingerprint,
    assert_fingerprint_unchanged,
    park_seeded,
    resume_seeded,
)
from .seedutils import seeded_vm

# Core images are only published for amd64, so these scenarios can't launch anywhere
# else. platform.machine() reports the host arch as x86_64 (Linux/macOS) or AMD64
# (Windows); everything else (aarch64, arm64, ...) is skipped.
pytestmark = pytest.mark.skipif(
    platform.machine().lower() not in ("x86_64", "amd64"),
    reason=f"core images are amd64-only (host arch `{platform.machine()}`)",
)


def _seed_core(vm, image, record):
    with seeded_vm(vm, image=image):
        sentinel = seed_sentinel(vm, vm)
        fingerprint = info_fingerprint(vm)  # cpu/memory only reported while running
        park_seeded(vm, how="stop", expected="Stopped")
    record.update({"sentinel": sentinel, "fingerprint": fingerprint})


def _verify_core(vm, record):
    resume_seeded(vm, expected_state="Stopped")
    assert_fingerprint_unchanged(vm, record["fingerprint"])
    assert_sentinel_record(vm, record["sentinel"])


@pytest.mark.seed
@pytest.mark.scenario("upg-core24")
def test_core24_seed(scenario):
    _seed_core("upg-core24", "core24", scenario.record)


@pytest.mark.verify
@pytest.mark.scenario("upg-core24")
def test_core24_verify(scenario):
    _verify_core("upg-core24", scenario.record)


@pytest.mark.seed
@pytest.mark.scenario("upg-core26")
def test_core26_seed(request):
    # Runtime gate (core26 lands in 1.16.3), not a marker: a `@requires_multipass`
    # marker would spawn `multipass` at collection time. Skipping before resolving
    # `scenario` keeps the seed manifest clean.
    skip_if_feature_not_supported("core26_images")
    scenario = request.getfixturevalue("scenario")
    _seed_core("upg-core26", "core26", scenario.record)


@pytest.mark.verify
@pytest.mark.scenario("upg-core26")
def test_core26_verify(scenario):
    # No version gate on verify: verify runs against the upgraded (to) version,
    # and `verify_scenario` skips scenarios absent from the manifest.
    _verify_core("upg-core26", scenario.record)
