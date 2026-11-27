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

# Installs the Flutter SDK version pinned in .fvmrc through FVM (https://fvm.app)
# and stores its location in MULTIPASS_FLUTTER_SDK_DIR.
function(setup_flutter_sdk)
  set(FVMRC_PATH "${CMAKE_SOURCE_DIR}/.fvmrc")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${FVMRC_PATH}")

  file(READ "${FVMRC_PATH}" FVMRC_CONTENTS)
  string(JSON FLUTTER_VERSION ERROR_VARIABLE FVMRC_ERROR GET "${FVMRC_CONTENTS}" flutter)
  if(FVMRC_ERROR)
    message(FATAL_ERROR "Could not read the Flutter version from ${FVMRC_PATH}: ${FVMRC_ERROR}")
  endif()

  # find_program() won't search again while a cached path is set, so drop it if FVM was moved
  if(FVM_EXECUTABLE AND NOT EXISTS "${FVM_EXECUTABLE}")
    unset(FVM_EXECUTABLE CACHE)
  endif()
  find_program(FVM_EXECUTABLE fvm)
  if(NOT FVM_EXECUTABLE)
    message(FATAL_ERROR
      "FVM is required to build the Flutter GUI. Install it following "
      "https://fvm.app/documentation/getting-started/installation, "
      "or configure with -DMULTIPASS_ENABLE_FLUTTER_GUI=OFF.")
  endif()

  message(STATUS "Installing Flutter ${FLUTTER_VERSION} with FVM")
  execute_process(COMMAND ${FVM_EXECUTABLE} install ${FLUTTER_VERSION} --skip-pub-get
                  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                  COMMAND_ERROR_IS_FATAL ANY)

  execute_process(COMMAND ${FVM_EXECUTABLE} api list --compress --skip-size-calculation
                  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                  OUTPUT_VARIABLE FVM_VERSIONS_JSON
                  COMMAND_ERROR_IS_FATAL ANY)
  string(JSON FVM_VERSION_COUNT ERROR_VARIABLE FVM_JSON_ERROR LENGTH "${FVM_VERSIONS_JSON}" versions)
  if(FVM_JSON_ERROR)
    message(FATAL_ERROR "Could not parse the output of 'fvm api list': ${FVM_JSON_ERROR}")
  endif()

  set(FLUTTER_SDK_DIR "")
  if(FVM_VERSION_COUNT GREATER 0)
    math(EXPR FVM_LAST_INDEX "${FVM_VERSION_COUNT} - 1")
    foreach(INDEX RANGE ${FVM_LAST_INDEX})
      string(JSON VERSION_NAME GET "${FVM_VERSIONS_JSON}" versions ${INDEX} name)
      if(VERSION_NAME STREQUAL FLUTTER_VERSION)
        string(JSON FLUTTER_SDK_DIR GET "${FVM_VERSIONS_JSON}" versions ${INDEX} directory)
        break()
      endif()
    endforeach()
  endif()

  if(NOT FLUTTER_SDK_DIR)
    message(FATAL_ERROR "FVM did not report an installation of Flutter ${FLUTTER_VERSION}")
  endif()

  file(TO_CMAKE_PATH "${FLUTTER_SDK_DIR}" FLUTTER_SDK_DIR)
  message(STATUS "Using Flutter SDK: ${FLUTTER_SDK_DIR}")
  set(MULTIPASS_FLUTTER_SDK_DIR "${FLUTTER_SDK_DIR}" CACHE INTERNAL "Flutter SDK pinned in .fvmrc")
endfunction()
