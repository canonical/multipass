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
default image (stopped), a non-default LTS (focal, suspended) and a non-Ubuntu
image (debian, stopped); the suspended case skips where unsupported."""

import pytest

from cli.config import cfg
from cli.multipass.feature_versions import requires_multipass
from .helpers import (
    seed_sentinel,
    assert_sentinel_record,
    info_fingerprint,
    assert_fingerprint_unchanged,
    park_seeded,
    resume_seeded,
)
from .seedutils import seeded_vm

requires_suspend = pytest.mark.skipif(
    cfg.driver in ("lxd", "applevz"),
    reason=f"suspend is not supported on the `{cfg.driver}` driver",
)


def _seed(vm, image, how, expected, record):
    with seeded_vm(vm, image=image) if image else seeded_vm(vm):
        sentinel = seed_sentinel(vm, vm)
        fingerprint = info_fingerprint(vm)  # cpu/memory only reported while running
        park_seeded(vm, how=how, expected=expected)
    record.update({"sentinel": sentinel, "fingerprint": fingerprint, "expected": expected})


def _verify(vm, record):
    resume_seeded(vm, expected_state=record["expected"])
    assert_fingerprint_unchanged(vm, record["fingerprint"])
    assert_sentinel_record(vm, record["sentinel"])


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
@requires_multipass(">=1.17")
def test_debian_seed(scenario):
    _seed("upg-debian", "debian", "stop", "Stopped", scenario.record)


@pytest.mark.verify
@pytest.mark.scenario("upg-debian")
@requires_multipass(">=1.17")
def test_debian_verify(scenario):
    _verify("upg-debian", scenario.record)
