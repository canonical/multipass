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

include(CMakeDependentOption)
include(src/cmake/environment-utils.cmake)

if(DEFINED MP_ALLOW_OPTIONAL_FEATURES)
  # If `MP_ALLOW_OPTIONAL_FEATURES` is already defined, make sure to cache it so we never
  # automatically change its value on subsequent runs of CMake.
  set(MP_ALLOW_OPTIONAL_FEATURES "${MP_ALLOW_OPTIONAL_FEATURES}" CACHE INTERNAL
      "allow optional features")
else()
  # Otherwise, just temporarily set its value based on whether we're on a release branch. This way,
  # if we change branches, the value will automatically update.
  is_release_branch(GIT_IS_RELEASE_BRANCH)
  if(GIT_IS_RELEASE_BRANCH)
    set(MP_ALLOW_OPTIONAL_FEATURES OFF)
  else()
    set(MP_ALLOW_OPTIONAL_FEATURES ON)
  endif()
endif()

# Define a feature flag named `option`, with optional requirements as the third argument (see
# `cmake_dependent_option` for details).
macro(feature_flag option doc)
  if(${ARGC} GREATER 2)
    cmake_dependent_option(${option} "${doc}" ON "MP_ALLOW_OPTIONAL_FEATURES;${ARGV2}" OFF)
  else()
    cmake_dependent_option(${option} "${doc}" ON MP_ALLOW_OPTIONAL_FEATURES OFF)
  endif()
endmacro()
