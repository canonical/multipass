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

"""Checks for resolving instances by name from the host, as <instance>.<local.dns-domain>."""

import functools
import socket
import sys
import time

from .feature_versions import multipass_version_has_feature
from .helpers import info, state

DEFAULT_DNS_DOMAIN = "multipass.local"

# The host caches answers: a name may keep resolving for a while after it stops being valid (up
# to the answer's TTL), or not resolve for a while after it becomes valid.
RESOLUTION_TIMEOUT = 90


@functools.cache
def resolution_by_name_supported():
    return sys.platform == "win32" and multipass_version_has_feature("dns_domain")


def resolve(name):
    """The IPv4 addresses the host resolves `name` to, the way applications do."""
    try:
        return {
            entry[4][0]
            for entry in socket.getaddrinfo(name, None)
            if entry[0] == socket.AF_INET
        }
    except socket.gaierror:
        return set()


def eventually(condition, timeout=RESOLUTION_TIMEOUT):
    deadline = time.monotonic() + timeout
    while not condition():
        if time.monotonic() > deadline:
            return False
        time.sleep(1)
    return True


def running_ipv4(instance):
    assert eventually(
        lambda: state(instance) == "Running" and info(instance)[instance]["ipv4"]
    ), f"{instance} has no IPv4 address"
    return info(instance)[instance]["ipv4"][0]


def assert_resolves_by_name(instance, domain=DEFAULT_DNS_DOMAIN):
    """Where supported, asserts that `instance` resolves to its IPv4 address."""
    if not resolution_by_name_supported():
        return

    name = f"{instance}.{domain}"
    address = running_ipv4(instance)
    assert eventually(
        lambda: address in resolve(name)
    ), f"{name} does not resolve to {address}"


def assert_does_not_resolve_by_name(instance, domain=DEFAULT_DNS_DOMAIN):
    """Where supported, asserts that `instance` does not resolve."""
    if not resolution_by_name_supported():
        return

    name = f"{instance}.{domain}"
    assert eventually(lambda: not resolve(name)), f"{name} still resolves"
