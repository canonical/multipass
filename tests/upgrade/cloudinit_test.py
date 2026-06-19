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

from cli.multipass import launch, multipass, state, read_file, get_cloudinit_instance_id
from .helpers import make_sentinel
from .seedutils import ensure_absent, daemon_readable_dir

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
def test_cloudinit_seed(seed_manifest):
    content = make_sentinel("cloudinit")

    # The cloud-init file is read by the (confined) Multipass client at launch,
    # so it must live somewhere the snap can see -- not host /tmp.
    ci_path = daemon_readable_dir(VM) / "cloud-init.yaml"
    ci_path.write_text(
        CLOUD_INIT_TEMPLATE.format(marker=MARKER_PATH, content=content), encoding="utf-8"
    )

    ensure_absent(VM)
    with launch(cfg_override={
        "name": VM,
        "autopurge": False,
        "extra_args": ["--cloud-init", str(ci_path)],
    }):
        assert read_file(VM, MARKER_PATH).strip() == content, (
            "cloud-init marker was not provisioned as expected"
        )
        instance_id = get_cloudinit_instance_id(VM).strip()
        assert multipass("stop", VM)
        assert state(VM) == "Stopped"

    seed_manifest[VM] = {
        "marker_path": MARKER_PATH,
        "marker_content": content,
        "instance_id": instance_id,
    }


@pytest.mark.verify
@pytest.mark.purge(VM)
def test_cloudinit_verify(verify_manifest):
    recorded = verify_manifest[VM]

    assert state(VM) == "Stopped"
    assert multipass("start", VM)
    assert state(VM) == "Running"

    # The provisioned marker persists byte for byte...
    assert read_file(VM, recorded["marker_path"]).strip() == recorded["marker_content"], (
        "cloud-init marker changed or was lost across upgrade"
    )
    # ...and the instance id is unchanged, proving cloud-init did not re-run.
    assert get_cloudinit_instance_id(VM).strip() == recorded["instance_id"], (
        "cloud-init instance id changed across upgrade (cloud-init re-ran?)"
    )
