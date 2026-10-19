#!/usr/bin/env python3
"""Build a NoCloud cloud-init seed ISO whose guest advertises a given DHCP hostname.

Used by the Hyper-V HCN DNS end-to-end integration tests (the
``HyperV_ComponentIntegrationTests.dns_suffix_*`` tests in
``tests/unit/hcs/test_bb_cit_hyperv.cpp``).

The guest advertises ``<hostname>`` as its DHCP option-12 host name, so that the
ICS DNS proxy registers it as ``<hostname>.<ICSDomain>``. A *dotted* hostname
(e.g. ``mp-fqdn-ab12cd.multipass``) is stored as ``...multipass.mshome.net``
without any change to the global ``ICSDomain``; see README.md.

A *unique* hostname is generated per test run so the ICS proxy never serves a
stale registration from a previous run (its DHCP/DNS registrations persist and
cannot be flushed without stopping SharedAccess, which HNS pins).

Requires pycdlib (``python -m pip install pycdlib``).

Usage:
    python make_seed_iso.py --hostname <hostname> --output <iso-path>
"""
import argparse
import io

import pycdlib

USER_DATA_TEMPLATE = """\
#cloud-config
chpasswd:
  list: |
     root:password
     alpine:password
  expire: False
# Stock Alpine's busybox udhcpc does not send DHCP option 12, so re-run it with the
# hostname once the network is up. The ICS DNS proxy then registers
# "<hostname>.<ICSDomain>"; a dotted hostname leaves the global ICSDomain untouched.
runcmd:
  - sh -c 'HN={hostname}; for i in 1 2 3 4 5 6; do udhcpc -i eth0 -x hostname:$HN -n -q && break; sleep 3; done'
"""

META_DATA_TEMPLATE = """\
local-hostname: {local_hostname}
cloud-name: proof-of-concept
"""


def build(hostname: str, output: str) -> None:
    # meta-data local-hostname must be a single DNS label; strip the dotted part.
    local_hostname = hostname.split(".", 1)[0]
    files = {
        "user-data": USER_DATA_TEMPLATE.format(hostname=hostname),
        "meta-data": META_DATA_TEMPLATE.format(local_hostname=local_hostname),
    }
    iso = pycdlib.PyCdlib()
    iso.new(interchange_level=3, joliet=3, rock_ridge="1.09", vol_ident="cidata")
    for name, text in files.items():
        data = text.encode("utf-8")
        iso.add_fp(
            io.BytesIO(data),
            len(data),
            "/" + name.upper().replace("-", "_") + ".;1",
            rr_name=name,
            joliet_path="/" + name,
        )
    iso.write(output)
    iso.close()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hostname", required=True, help="DHCP host name to advertise (may be dotted)")
    parser.add_argument("--output", required=True, help="path to write the seed ISO to")
    args = parser.parse_args()
    build(args.hostname, args.output)


if __name__ == "__main__":
    main()
