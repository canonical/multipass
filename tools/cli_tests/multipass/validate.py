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

from collections.abc import Sequence
from pprint import pformat

from cli_tests.multipass import multipass


def validate_output(*args, properties, jq_filter=None):
    with multipass(*args).json() as output:
        assert output.exitstatus == 0
        result = output.jq(jq_filter).all() if jq_filter else output
        assert len(result) == 1
        [instance] = result
        for k, v in properties.items():
            assert k in instance, f"Missing key '{k}' in instance:\n{pformat(instance)}"
            # A property can be assigned to a callable. In that case, we'd want
            # to treat it as a predicate and call it. We'd call it for each
            # element if the matching key's value is a collection.
            if callable(v):
                if isinstance(instance[k], Sequence):
                    for item in instance[k]:
                        assert v(item)
                else:
                    assert v(instance[k])
            else:
                assert instance[k] == v, (
                    f"\n❌ Assertion failed for key '{k}':\n"
                    f"  Expected: {v!r} (type: {type(v).__name__})\n"
                    f"  Actual:   {instance[k]!r} (type: {type(instance[k]).__name__})\n"
                )


def validate_list_output(name, properties):
    """Validate properties of a specific VM from `multipass list`.

    Fetches all VM data via `multipass list --format=json`, locates the
    VM by name, and asserts that its properties match the expected ones.

    Args:
        name (str): Name of the VM to validate.
        properties (dict): Expected properties. Values can be:
            - Literals (compared with equality)
            - Callables (used as predicates)
              - If the VM's value is a sequence, the predicate is applied to each item.

    Raises:
        AssertionError: If the VM is missing, or any property fails validation.
    """

    return validate_output(
        "list",
        "--format=json",
        properties=properties,
        jq_filter=f'.list[] | select(.name=="{name}")',
    )


def validate_info_snapshot_output(name, properties):
    """Validate properties of a specific VM from `multipass list`.

    Fetches all VM data via `multipass list --format=json`, locates the
    VM by name, and asserts that its properties match the expected ones.

    Args:
        name (str): Name of the VM to validate.
        properties (dict): Expected properties. Values can be:
            - Literals (compared with equality)
            - Callables (used as predicates)
              - If the VM's value is a sequence, the predicate is applied to each item.

    Raises:
        AssertionError: If the VM is missing, or any property fails validation.
    """

    return validate_output(
        "info",
        "--format=json",
        "--snapshots",
        f"{name}",
        properties=properties,
        jq_filter=f'.info["{name}"]',
    )


def validate_info_output(name, properties):
    return validate_output(
        "info",
        "--format=json",
        f"{name}",
        properties=properties,
        jq_filter=f'.info["{name}"]',
    )
