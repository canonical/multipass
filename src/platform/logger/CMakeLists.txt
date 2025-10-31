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

if(MSVC)
  add_library(wineventlogger STATIC win_event_logger.cpp)
  qt6_disable_unicode_defines(wineventlogger)
  target_link_libraries(wineventlogger PUBLIC fmt::fmt-header-only Qt6::Core)
elseif(APPLE)
  # Platform specific logger library here
else()
  add_library(logger_linux_common STATIC linux_logger.cpp)
  target_link_libraries(logger_linux_common PUBLIC Qt6::Core)

  # Determine whether journald is available.
  find_package(PkgConfig)
  pkg_check_modules(JOURNALD libsystemd)

  # Declare the journald target if found.
  if(JOURNALD_FOUND)
    set(JOURNALD_FOUND "${JOURNALD_FOUND}" PARENT_SCOPE)
    add_library(logger_linux_journald STATIC journald_wrapper.cpp journald_logger.cpp)
    target_include_directories(logger_linux_journald PRIVATE ${JOURNALD_INCLUDE_DIRS})
    target_link_libraries(logger_linux_journald PUBLIC logger_linux_common ${JOURNALD_LIBRARIES})
  endif()


  # Determine if syslog is available.
  check_include_files(syslog.h HAVE_SYSLOG_H)
  check_function_exists(syslog HAVE_SYSLOG)

  set(SYSLOG_FOUND false)
  if(HAVE_SYSLOG_H AND HAVE_SYSLOG)
      set(SYSLOG_FOUND true)
  endif()

  # Declare the syslog target if found.
  if(SYSLOG_FOUND)
    add_library(logger_linux_syslog STATIC syslog_wrapper.cpp syslog_logger.cpp)
    target_link_libraries(logger_linux_syslog PUBLIC logger_linux_common)
  endif()

  # Settle on which logging backend to use
  if(MULTIPASS_ENABLE_SYSLOG)
    add_library(logger_linux ALIAS logger_linux_syslog)
    message(STATUS "[logging] will use the syslog backend")
  elseif(JOURNALD_FOUND)
    add_library(logger_linux ALIAS logger_linux_journald)
    message(STATUS "[logging] will use the journald backend")
  else()
    message(FATAL_ERROR "No viable logging backend found in the environment!")
  endif()
endif()
