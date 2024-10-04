# Copyright (C) Canonical, Ltd.
#
# This program is free software: you can redistribute it and/or modify it under
# the terms of the GNU General Public License version 3 as published by the Free
# Software Foundation.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program.  If not, see <http://www.gnu.org/licenses/>.

set(PRIMING_DIR
    "${CPACK_TEMPORARY_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}-priming")

file(
  GLOB subdirectories
  LIST_DIRECTORIES true
  "${CPACK_TEMPORARY_DIRECTORY}/*")

foreach(subdirectory ${subdirectories})
  file(COPY "${subdirectory}/" DESTINATION "${PRIMING_DIR}")
endforeach()

if (CPACK_BUILD_TYPE STREQUAL "Release" OR CPACK_BUILD_TYPE STREQUAL "RelWithDebInfo")
  set(CPACK_BUILD_TYPE "Release")
else()
  set(CPACK_BUILD_TYPE "Debug")
endif()

string(REGEX REPLACE "-[^.]+|\\+.+|\\.pr.+" "" BUILD_VERSION ${CPACK_PACKAGE_VERSION})

execute_process(
  COMMAND
    msbuild /Restore /verbosity:normal /p:RestorePackagesConfig=true 
    /p:Configuration=${CPACK_BUILD_TYPE}
    /p:PublishDir=${PRIMING_DIR}
    /p:OutputPath=${CPACK_OUTPUT_FILE_PREFIX}/packages
    /p:BuildVersion=${BUILD_VERSION}
    /p:OutputName=${CPACK_PACKAGE_FILE_NAME}
  WORKING_DIRECTORY "${CPACK_SOURCE_DIR}/packaging/windows/wix"
  OUTPUT_VARIABLE build_output
  RESULT_VARIABLE result)

file(REMOVE_RECURSE "${PRIMING_DIR}")

file(WRITE ${CPACK_TEMPORARY_DIRECTORY}\\..\\WiXOutput.log "${build_output}")

if(result)
  message(FATAL_ERROR "CPack failed to build WiX package: ${build_output}")
else()
  message(STATUS ${build_output})
endif()
