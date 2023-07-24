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

if (NOT MULTIPASS_ENABLE_SYSLOG)
  find_package(PkgConfig)
  pkg_check_modules(JOURNALD libsystemd)

  if (JOURNALD_FOUND)
    set(JOURNALD_FOUND "${JOURNALD_FOUND}" PARENT_SCOPE)
    add_library(logger_linux STATIC linux_logger.cpp journald_logger.cpp)
    target_include_directories(logger_linux PRIVATE
            ${JOURNALD_INCLUDE_DIRS})
    target_link_libraries(logger_linux fmt Qt6::Core ${JOURNALD_LIBRARIES})
  endif()
endif()

if (MULTIPASS_ENABLE_SYSLOG OR NOT JOURNALD_FOUND)
  check_include_files(syslog.h HAVE_SYSLOG_H)
  check_function_exists(syslog HAVE_SYSLOG)
  add_library(logger_linux STATIC linux_logger.cpp syslog_logger.cpp)
  target_link_libraries(logger_linux fmt Qt6::Core)
endif()
