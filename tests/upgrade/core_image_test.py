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
upgrade. Covered for `core:core24` and `core:core26` (both stopped); the
`core26` image only exists from 1.16.3, so its seed is gated.

Unlike the cloud images in `lifecycle_test`, these scenarios don't touch the
guest filesystem: Ubuntu Core is minimal and immutable and doesn't behave like a
standard cloud image for SSH/cloud-init, so a sentinel file is not a portable
signal here. The `image_release` fingerprint plus a park/resume round trip is."""

import pytest

from cli.multipass import multipass, state
from cli.multipass.feature_versions import requires_multipass
from .helpers import (
    info_fingerprint,
    assert_fingerprint_unchanged,
    park_seeded,
)
from .seedutils import seeded_vm


def _seed_core(vm, image, record):
    with seeded_vm(vm, image=image):
        fingerprint = info_fingerprint(vm)  # cpu/memory only reported while running
        park_seeded(vm, how="stop", expected="Stopped")
    record.update({"fingerprint": fingerprint})


def _verify_core(vm, record):
    # ponytail: identity + boot check only, no guest sentinel. Upgrade to the
    # full `lifecycle_test` sentinel flow if `multipass exec` is confirmed on Core.
    came_back = state(vm)
    assert came_back == "Stopped", f"`{vm}` came back as `{came_back}`, expected `Stopped`"
    assert multipass("start", vm)
    assert state(vm) == "Running"
    assert_fingerprint_unchanged(vm, record["fingerprint"])


@pytest.mark.seed
@pytest.mark.scenario("upg-core24")
def test_core24_seed(scenario):
    _seed_core("upg-core24", "core:core24", scenario.record)


@pytest.mark.verify
@pytest.mark.scenario("upg-core24")
def test_core24_verify(scenario):
    _verify_core("upg-core24", scenario.record)


@pytest.mark.seed
@pytest.mark.scenario("upg-core26")
@requires_multipass(">=1.16.3")
def test_core26_seed(scenario):
    _seed_core("upg-core26", "core:core26", scenario.record)


@pytest.mark.verify
@pytest.mark.scenario("upg-core26")
def test_core26_verify(scenario):
    # No version gate on verify: verify runs against the upgraded (to) version,
    # and `verify_scenario` skips scenarios absent from the manifest.
    _verify_core("upg-core26", scenario.record)
