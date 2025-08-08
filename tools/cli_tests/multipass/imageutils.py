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

"""Utility for translating image metadata."""


def image_name_to_version(image):
    """Return the Ubuntu version string for a given Multipass image codename."""

    mapping = {
        "noble": "24.04",
        "jammy": "22.04",
        "focal": "20.04",
        "bionic": "18.04",
    }
    try:
        return mapping[image]
    except KeyError as exc:
        raise ValueError(f"Unknown image codename: {image!r}") from exc
