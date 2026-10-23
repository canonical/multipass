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

"""Multipass command line tests for `set` and `get`"""

import sys
import pytest

from cli.multipass import (
    multipass,
    default_driver_name,
    multipass_version_has_feature,
    assert_resolves_by_name,
    assert_does_not_resolve_by_name,
)


@pytest.mark.settings
@pytest.mark.usefixtures("multipassd")
class TestSettings:
    def test_get_all_keys(self):
        expected_keys = [
            *(
                ["client.apps.windows-terminal.profiles"]
                if sys.platform == "win32"
                else []
            ),
            "client.primary-name",
            "local.bridged-network",
            *(
                ["local.dns-domain"]
                if multipass_version_has_feature("dns_domain")
                else []
            ),
            "local.driver",
            "local.image.mirror",
            "local.passphrase",
            "local.privileged-mounts",
        ]
        with multipass("get", "--keys") as keys:
            assert keys
            keys_split = keys.content.split()
            assert keys_split == expected_keys

    def test_get_disabled_primary_name(self):
        assert multipass("set", "client.primary-name=")
        assert multipass("get", "client.primary-name") == "<empty>"

    def test_set_primary_name(self):
        assert multipass("set", "client.primary-name=foo")
        assert multipass("get", "client.primary-name") == "foo"

    def test_set_driver_name_to_default(self, multipassd):
        assert multipass("set", f"local.driver={default_driver_name()}")
        # Daemon will autorestart here. We should wait until it's back.
        multipassd.wait_for_restart()
        assert multipass("get", "local.driver") == default_driver_name()

    def test_dns_domain_defaults_to_multipass_local(self, dns_domain):
        assert multipass("get", "local.dns-domain") == "multipass.local"

    def test_set_dns_domain(self, dns_domain):
        dns_domain("Mp-Test.Local")
        assert multipass("get", "local.dns-domain") == "mp-test.local"

    def test_set_empty_dns_domain(self, dns_domain):
        dns_domain("")
        assert multipass("get", "local.dns-domain") == "<empty>"

    @pytest.mark.parametrize(
        "domain",
        [
            ".mp.local",
            "mp.local.",
            "mp..local",
            "-mp.local",
            "mp_test.local",
            f"{'a' * 64}.local",
        ],
    )
    def test_set_invalid_dns_domain(self, dns_domain, domain):
        with multipass("set", f"local.dns-domain={domain}") as output:
            assert not output
            assert "Invalid DNS domain" in output
        assert multipass("get", "local.dns-domain") == "multipass.local"

    @pytest.mark.dns
    def test_instances_resolve_under_the_dns_domain(self, dns_domain, instance):
        dns_domain("mp-cli-test.internal")
        assert_resolves_by_name(instance, "mp-cli-test.internal")
        assert_does_not_resolve_by_name(instance)  # under the previous domain

        dns_domain("")
        assert_does_not_resolve_by_name(instance, "mp-cli-test.internal")
