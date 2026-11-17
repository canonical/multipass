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

from typing import Optional

from cli.config import cfg
from .multipassd_governor import MultipassdGovernor
from .multipassd_controller import MultipassdController
from .standalone_multipassd_controller import StandaloneMultipassdController
from .snap_multipassd_controller import SnapMultipassdController
from .winsvc_multipassd_controller import WindowsServiceMultipassdController
from .launchd_multipassd_controller import LaunchdMultipassdController
from .controller_exceptions import ControllerPrerequisiteError

#: `--daemon-controller` name -> controller class.
CONTROLLERS = {
    "standalone": StandaloneMultipassdController,
    "snap": SnapMultipassdController,
    "launchd": LaunchdMultipassdController,
    "winsvc": WindowsServiceMultipassdController,
}


def get_temp_root() -> Optional[str]:
    """`get_temp_root()` of the configured daemon controller, or None for the
    system default (also when no controller is in use)."""
    controller = CONTROLLERS.get(cfg.daemon_controller)
    return controller.get_temp_root() if controller else None
