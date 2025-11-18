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

from packaging import version as ver
from .helpers import get_multipass_version

import pytest


def _feature_version(name):
    feats = {
        "appliances": {"min": ver.parse("1"), "max": ver.parse("1.16")},
        "blueprints": {"max": ver.parse("1.16")},
        "debian_images": {"min": ver.parse("1.17")},
        "fedora_images": {"min": ver.parse("1.17")},
        "wait_ready": {"min": ver.parse("1.17")},
    }
    assert name in feats, f"No such feature: {name}"
    return feats[name]


def multipass_version_has_feature(feature_name, version=None):
    """Check if a particular version of Multipass has the specified feature.
    Version is set to current by default."""

    if not version:
        version = get_multipass_version()

    fv = _feature_version(feature_name)

    if "min" in fv:
        if not version >= fv["min"]:
            return False

    if "max" in fv:
        if not version <= fv["max"]:
            return False

    return True


def skip_if_feature_not_supported(feature_name, version=None):
    if not multipass_version_has_feature(feature_name, version=version):
        pytest.skip(
            f"The version does not support `{feature_name}`, skipping.")
