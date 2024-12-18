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

cmake_minimum_required(VERSION 3.20)

foreach(PACKAGE IN LISTS CPACK_PACKAGE_FILES)
    cmake_path(GET PACKAGE PARENT_PATH PACKAGE_DIR)
    execute_process(
        COMMAND "${CPACK_MODULE_PATH}/sign-and-notarize.sh" --app-signer - ${PACKAGE}
        WORKING_DIRECTORY ${PACKAGE_DIR}
    )
endforeach()
