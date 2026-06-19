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

"""State applied by a custom cloud-init at first boot should survive an upgrade.

cloud-init runs once, keyed by the instance id. After an upgrade the VM keeps the
same instance id, so cloud-init must not re-run and the files it wrote must
remain exactly as provisioned."""

import pytest

from cli.multipass import read_file, get_cloudinit_instance_id
from .helpers import make_sentinel, assert_sentinel_record, park_seeded, resume_seeded
from .seedutils import seeded_vm, daemon_readable_dir

VM = "upg-cloudinit"
MARKER_PATH = "/etc/upgrade-cloudinit-marker"

CLOUD_INIT_TEMPLATE = """\
#cloud-config
write_files:
  - path: {marker}
    permissions: '0644'
    content: "{content}"
"""


@pytest.mark.seed
@pytest.mark.scenario(VM)
def test_cloudinit_seed(seed_scenario):
    content = make_sentinel("cloudinit")

    # The cloud-init file is read by the (confined) Multipass client at launch,
    # so it must live somewhere the snap can see -- not host /tmp.
    ci_path = daemon_readable_dir(VM) / "cloud-init.yaml"
    ci_path.write_text(
        CLOUD_INIT_TEMPLATE.format(marker=MARKER_PATH, content=content), encoding="utf-8"
    )

    with seeded_vm(VM, extra_args=["--cloud-init", str(ci_path)]):
        assert read_file(VM, MARKER_PATH).strip() == content, (
            "cloud-init marker was not provisioned as expected"
        )
        instance_id = get_cloudinit_instance_id(VM).strip()
        park_seeded(VM)

    seed_scenario.record.update({
        "marker": {"path": MARKER_PATH, "content": content},
        "instance_id": instance_id,
    })


@pytest.mark.verify
@pytest.mark.scenario(VM)
def test_cloudinit_verify(verify_scenario):
    recorded = verify_scenario.record

    resume_seeded(VM, expected_state="Stopped")

    # The provisioned marker persists byte for byte...
    assert_sentinel_record(VM, recorded["marker"])
    # ...and the instance id is unchanged, proving cloud-init did not re-run.
    assert get_cloudinit_instance_id(VM).strip() == recorded["instance_id"], (
        "cloud-init instance id changed across upgrade (cloud-init re-ran?)"
    )
