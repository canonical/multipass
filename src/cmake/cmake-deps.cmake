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

include(FetchContent)

# Declare and fetch out_ptr
FetchContent_Declare(
  out_ptr
  GIT_REPOSITORY https://github.com/soasis/out_ptr.git
  GIT_TAG 02a577edfcf25e2519e380a95c16743b7e5878a1
)

FetchContent_MakeAvailable(out_ptr)
