# Copyright (C) Canonical, Ltd.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

file(REMOVE_RECURSE "${CPACK_TEMPORARY_INSTALL_DIRECTORY}/multipassd${CPACK_PACKAGING_INSTALL_PREFIX}/lib/pkgconfig")
file(REMOVE "${CPACK_TEMPORARY_INSTALL_DIRECTORY}/multipassd${CPACK_PACKAGING_INSTALL_PREFIX}/lib/libgtest.a")
file(REMOVE "${CPACK_TEMPORARY_INSTALL_DIRECTORY}/multipassd${CPACK_PACKAGING_INSTALL_PREFIX}/lib/libgtest_main.a")
file(REMOVE "${CPACK_TEMPORARY_INSTALL_DIRECTORY}/multipassd${CPACK_PACKAGING_INSTALL_PREFIX}/lib/libgmock.a")
file(REMOVE "${CPACK_TEMPORARY_INSTALL_DIRECTORY}/multipassd${CPACK_PACKAGING_INSTALL_PREFIX}/lib/libgmock_main.a")
