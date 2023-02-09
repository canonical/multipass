#Copyright (C) Canonical, Ltd.
#
#This program is free software : you can redistribute it and / or modify
#it under the terms of the GNU General Public License version 3 as
#published by the Free Software Foundation.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
#GNU General Public License for more details.
#
#You should have received a copy of the GNU General Public License
#along with this program.If not, see < http: // www.gnu.org/licenses/>.
#

find_package(PkgConfig)
pkg_check_modules(JOURNALD libsystemd REQUIRED)

add_library(journaldlogger STATIC journald_logger.cpp)
target_include_directories(journaldlogger PRIVATE
        ${JOURNALD_INCLUDE_DIRS})
target_link_libraries(journaldlogger fmt Qt5::Core ${JOURNALD_LIBRARIES})
