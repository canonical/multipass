# Copyright © 2017-2019 Canonical Ltd.
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

if(MSVC)
  add_subdirectory(hyperv)
elseif(APPLE)
  add_subdirectory(hyperkit)
else()
  add_subdirectory(libvirt)
  add_subdirectory(lxd)
  add_subdirectory(qemu)
endif()

add_subdirectory(shared)
add_subdirectory(virtualbox)
