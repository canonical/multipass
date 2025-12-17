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

is_release_branch(GIT_IS_RELEASE_BRANCH)
if(GIT_IS_RELEASE_BRANCH)
  set(OPTIONAL_FEATURE_DEFAULT OFF)
else()
  set(OPTIONAL_FEATURE_DEFAULT ON)
endif()
option(MP_ALLOW_OPTIONAL_FEATURES "allow enabling optional features" ${OPTIONAL_FEATURE_DEFAULT})

# Define a feature flag named `option`, with optional requirements as the third argument (see
# `cmake_dependent_option` for details).
macro(feature_flag option doc)
  if(${ARGC} GREATER 2)
    cmake_dependent_option(${option} "${doc}" ON "MP_ALLOW_OPTIONAL_FEATURES;${ARGV2}" OFF)
  else()
    cmake_dependent_option(${option} "${doc}" ON MP_ALLOW_OPTIONAL_FEATURES OFF)
  endif()
endmacro()
