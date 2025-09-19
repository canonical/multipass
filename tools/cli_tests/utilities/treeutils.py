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

from collections.abc import Mapping


def find_lineage(tree, target):
    """Return path of keys from root to target in a nested dict, or None if not found."""

    def dfs(node, path):
        for key, child in node.items():
            new_path = path + [key]
            if key == target:
                return new_path
            result = dfs(child, new_path)
            if result:
                return result
        return None

    return dfs(tree, [])


def merge(dst: dict, src: Mapping) -> dict:
    """Recursively update dict `dst` with `src` (like .update(), but nested)."""

    for k, v in src.items():
        if k in dst and isinstance(dst[k], Mapping) and isinstance(v, Mapping):
            merge(dst[k], v)
        else:
            dst[k] = v
    return dst
