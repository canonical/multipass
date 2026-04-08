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

if(UNIX)
    list(APPEND MULTIPASS_BACKENDS qemu)

    if (FORCE_ENABLE_VIRTUALBOX OR (APPLE AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64"))
        list(APPEND MULTIPASS_BACKENDS virtualbox)
    endif()

    if(APPLEVZ_ENABLED)
        list(APPEND MULTIPASS_BACKENDS applevz)
    endif()
elseif(WIN32)
    set(MULTIPASS_BACKENDS hyperv virtualbox)
else()
    message(WARNING "No suitable backend exists for the platform!")
endif()

message(STATUS "Enabled backends: ${MULTIPASS_BACKENDS}")