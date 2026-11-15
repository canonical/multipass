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

include(src/cmake/feature-flag.cmake)

feature_flag(MULTIPASS_ENABLE_INSIGHTS "Enable Ubuntu Insights")

if(MULTIPASS_ENABLE_INSIGHTS)
  list(APPEND VCPKG_MANIFEST_FEATURES "ubuntu-insights")

  # vcpkg uses an isolated build environment, so explicitly preserve Go's path.
  find_program(GO_EXECUTABLE go REQUIRED)
  get_filename_component(GO_EXECUTABLE_DIR "${GO_EXECUTABLE}" DIRECTORY)

  # Set GO_EXECUTABLE_DIR environment variable, and ask vcpkg to keep it.
  # NOTE: The resulting ubuntu-insights ABI hash will not depend on the go toolchain
  # used to build it. So if a new go compiler is installed, the port will not be rebuilt.
  # This is fine for now.
  set(ENV{GO_EXECUTABLE_DIR} "${GO_EXECUTABLE_DIR}")

  set(VCPKG_KEEP_ENV_VARS "$ENV{VCPKG_KEEP_ENV_VARS}")
  list(APPEND VCPKG_KEEP_ENV_VARS GO_EXECUTABLE_DIR)
  list(REMOVE_DUPLICATES VCPKG_KEEP_ENV_VARS)

  set(ENV{VCPKG_KEEP_ENV_VARS} "${VCPKG_KEEP_ENV_VARS}")
  unset(VCPKG_KEEP_ENV_VARS)
endif()
