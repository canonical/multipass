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

function(is_running_in_ci OUT_VAR)
    if(DEFINED ENV{GITHUB_ACTIONS} AND "$ENV{GITHUB_ACTIONS}" STREQUAL "true")
        set(${OUT_VAR} TRUE PARENT_SCOPE)
    else()
        set(${OUT_VAR} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(is_release_branch OUTPUT_VARIABLE)
  # use upstream repo as the authoritative reference when checking for release status
  # set -DMULTIPASS_UPSTREAM="" to use the local repository
  if(MULTIPASS_UPSTREAM)
    set(MULTIPASS_UPSTREAM_PREFIX "${MULTIPASS_UPSTREAM}/")
  endif()

  execute_process(COMMAND git describe --all --exact --match "${MULTIPASS_UPSTREAM_PREFIX}release/*"
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE GIT_RELEASE_BRANCH
                  OUTPUT_STRIP_TRAILING_WHITESPACE
                  ERROR_QUIET)

  string(REGEX MATCH "release/[0-9]+\\.[0-9]+" GIT_RELEASE_MATCH "${GIT_RELEASE_BRANCH}")
  if(GIT_RELEASE_MATCH)
    set(${OUTPUT_VARIABLE} TRUE PARENT_SCOPE)
  else()
    set(${OUTPUT_VARIABLE} FALSE PARENT_SCOPE)
  endif()
endfunction()
