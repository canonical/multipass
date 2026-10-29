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

"""Version upgrade tests.

A two-phase suite, selected by marker, glued together by the CI:

    pytest tests/upgrade -m seed       # provision VMs on the installed version X
    # ... CI swaps Multipass X -> Y, instance storage preserved (no reboot) ...
    pytest tests/upgrade -m verify     # assert the seeded VMs survived

Each concern is a pair of ordinary pytest tests (a ``seed`` test and a
``verify`` test) living in ``<concern>_test.py``. The seed test stores what it
expects to survive in the manifest (the ``seed_manifest`` fixture); the verify
test reads it back (the ``verify_manifest`` fixture). This package only needs to
be importable so that pytest puts ``tests/`` on ``sys.path``.
"""
