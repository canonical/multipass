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

"""A VM's data and reported identity should survive an upgrade. Covered for the
default image (stopped), a non-default LTS (focal, suspended) and two non-Ubuntu
images (debian and fedora, stopped); the suspended case skips where unsupported."""

import pytest

from cli.config import cfg
from cli.multipass.feature_versions import skip_if_feature_not_supported
from .helpers import (
    seed_sentinel,
    assert_sentinel_record,
    info_fingerprint,
    assert_fingerprint_unchanged,
    park_seeded,
    resume_seeded,
    instance_ipv4,
    soft_assert,
    guest_identity,
    assert_guest_identity_unchanged,
)
from .seedutils import seeded_vm

requires_suspend = pytest.mark.skipif(
    cfg.driver in ("lxd", "applevz"),
    reason=f"suspend is not supported on the `{cfg.driver}` driver",
)


def _seed(vm, image, how, expected, record):
    with seeded_vm(vm, image=image) if image else seeded_vm(vm):
        sentinel = seed_sentinel(vm, vm)
        fingerprint = info_fingerprint(vm)  # cpu/memory/disk only reported while running
        identity = guest_identity(vm)  # guest-side identity, read while running
        ipv4 = instance_ipv4(vm)  # reported only while running
        park_seeded(vm, how=how, expected=expected)
    record.update(
        {
            "sentinel": sentinel,
            "fingerprint": fingerprint,
            "identity": identity,
            "expected": expected,
            "ipv4": ipv4,
        }
    )


def _verify(vm, record):
    resume_seeded(vm, expected_state=record["expected"])
    assert_fingerprint_unchanged(vm, record["fingerprint"])
    assert_guest_identity_unchanged(vm, record["identity"])
    assert_sentinel_record(vm, record["sentinel"])
    # The IP address may or may not survive an upgrade; warn on drift, don't fail.
    now = instance_ipv4(vm)
    soft_assert(
        now == record["ipv4"],
        f"IPv4 for `{vm}` changed across upgrade: {record['ipv4']} -> {now} "
        "(not treated as a failure -- review if unexpected)",
    )


@pytest.mark.seed
@pytest.mark.scenario("upg-lifecycle")
def test_lifecycle_seed(scenario):
    _seed("upg-lifecycle", None, "stop", "Stopped", scenario.record)


@pytest.mark.verify
@pytest.mark.scenario("upg-lifecycle")
def test_lifecycle_verify(scenario):
    _verify("upg-lifecycle", scenario.record)


@pytest.mark.seed
@pytest.mark.scenario("upg-focal")
@requires_suspend
def test_focal_seed(scenario):
    _seed("upg-focal", "focal", "suspend", "Suspended", scenario.record)


@pytest.mark.verify
@pytest.mark.scenario("upg-focal")
@requires_suspend
def test_focal_verify(scenario):
    _verify("upg-focal", scenario.record)


@pytest.mark.seed
@pytest.mark.scenario("upg-debian")
def test_debian_seed(request):
    # Gate at runtime, not with a marker: `@requires_multipass(...)` evaluates the
    # installed version by spawning `multipass` at *collection* time, which aborts
    # bare collection where the daemon/binary isn't ready. Skipping before resolving
    # `scenario` also keeps the seed manifest clean (no empty entry for a skip).
    skip_if_feature_not_supported("debian_images")
    scenario = request.getfixturevalue("scenario")
    _seed("upg-debian", "debian", "stop", "Stopped", scenario.record)


@pytest.mark.verify
@pytest.mark.scenario("upg-debian")
def test_debian_verify(scenario):
    # No version gate: verify runs against the upgraded (to) version, which may
    # have the feature even when the seeded (from) version did not. Whether this
    # scenario was seeded is recorded by its presence in the manifest, and
    # `verify_scenario` skips it when absent.
    _verify("upg-debian", scenario.record)


@pytest.mark.seed
@pytest.mark.scenario("upg-fedora")
def test_fedora_seed(request):
    skip_if_feature_not_supported("fedora_images")  # see test_debian_seed
    scenario = request.getfixturevalue("scenario")
    _seed("upg-fedora", "fedora", "stop", "Stopped", scenario.record)


@pytest.mark.verify
@pytest.mark.scenario("upg-fedora")
def test_fedora_verify(scenario):
    # No version gate on verify: see `test_debian_verify`.
    _verify("upg-fedora", scenario.record)
