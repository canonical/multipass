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

option(MP_ALLOW_OPTIONAL_FEATURES "allow enabling optional features" ON)

macro(feature_flag option doc)
  if(${ARGC} GREATER 2)
    cmake_dependent_option(${option} "${doc}" ON "MP_ALLOW_OPTIONAL_FEATURES;${ARGV2}" OFF)
  else()
    cmake_dependent_option(${option} "${doc}" ON MP_ALLOW_OPTIONAL_FEATURES OFF)
  endif()
endmacro()
