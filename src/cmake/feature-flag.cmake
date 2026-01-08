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

macro(feature_flag option doc)
  list(APPEND MP_ALL_FEATURE_FLAGS ${option})
  if(${ARGC} GREATER 2)
    cmake_dependent_option(${option} "${doc}" ON "MP_ALLOW_OPTIONAL_FEATURES;${ARGV2}" OFF)
  else()
    cmake_dependent_option(${option} "${doc}" ON MP_ALLOW_OPTIONAL_FEATURES OFF)
  endif()
endmacro()

function(is_custom_feature_set OUT_VAR)
  set(${OUT_VAR} FALSE PARENT_SCOPE)
  foreach(feature IN LISTS MP_ALL_FEATURE_FLAGS)
    # Check if the feature is available but not enabled, in which case we have a custom feature set.
    # (Note that when disabling `MP_ALLOW_OPTIONAL_FEATURES`, all the feature flags are marked
    # unavailable, so this condition will always be false then.)
    if(${${feature}_AVAILABLE} AND NOT ${${feature}})
      set(${OUT_VAR} TRUE PARENT_SCOPE)
      return()
    endif()
  endforeach()
endfunction()
