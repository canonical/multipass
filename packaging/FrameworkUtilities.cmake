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

include(GetPrerequisites)

#
# get_binary_rpaths(BINARY RPATHS_VAR)
#
# Analyses the $BINARY and places a list of rpaths specified in $RPATHS_VAR_OUT
#
function(get_binary_rpaths binary rpaths_var)
  cmake_parse_arguments(RPATHS "" "LOADER_PATH" "" ${ARGN})

  find_program(otool_cmd "otool")
  mark_as_advanced(otool_cmd)

  if(otool_cmd)
    execute_process(
      COMMAND "${otool_cmd}" -l "${binary}"
      OUTPUT_VARIABLE load_cmds_ov
      )
    string(REGEX REPLACE "[^\n]+cmd LC_RPATH\n[^\n]+\n[^\n]+path ([^\n]+) \\(offset[^\n]+\n" "rpath \\1\n" load_cmds_ov "${load_cmds_ov}")
    if(RPATHS_LOADER_PATH)
      string(REGEX REPLACE "rpath @loader_path" "rpath ${RPATHS_LOADER_PATH}" load_cmds_ov "${load_cmds_ov}")
    else()
      string(REGEX REPLACE "rpath @loader_path[^\n]+" "" load_cmds_ov "${load_cmds_ov}")
    endif()
    string(REGEX REPLACE "rpath @executable_path[^\n]+" "" load_cmds_ov "${load_cmds_ov}")
    string(REGEX MATCHALL "rpath [^\n]+" load_cmds_ov "${load_cmds_ov}")
    string(REGEX REPLACE "rpath " "" load_cmds_ov "${load_cmds_ov}")
    if(load_cmds_ov)
      foreach(rpath ${load_cmds_ov})
        gp_append_unique(${rpaths_var} "${rpath}")
      endforeach()
    endif()
  endif()

  set(${rpaths_var} ${${rpaths_var}} PARENT_SCOPE)
endfunction()


#
# install_frameworks(BINARY FRAMEWORK_DESTINATION)
#
# Analyses the $BINARY to determine all the frameworks/libraries it has an rpath dependency on.
# Then it tracks down those frameworks/libraries on the filesystem and copies them (preserving the
# rpath-specified directory structure) into the $FRAMEWORK_DESTINATION
#
# Note: this does not update the rpath specified in $BINARY. Better to use:
#      set_target_properties( <target> PROPERTIES INSTALL_RPATH "@executable_path/../lib" )
#
function(install_frameworks binary framework_destination)
  message(STATUS "Attempting to auto-copy all the $rpath linked frameworks of ${binary} into the install directory ${framework_destination}")
  get_binary_rpaths("${binary}" BINARY_RPATHS ${ARGN})
  get_prerequisites("${binary}" RPATHED_LIBS 1 1 0 "" "${BINARY_RPATHS}")

  foreach(rpath ${BINARY_RPATHS})
    foreach(lib ${RPATHED_LIBS})
      string(REPLACE "@rpath" "" lib "${lib}")
      message(STATUS "Checking for ${rpath}${lib}")
      if(EXISTS "${rpath}${lib}")
        message(STATUS "copying COMMAND ${CMAKE_COMMAND} -E copy ${rpath}${lib} ${framework_destination}${lib}")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy "${rpath}${lib}" "${framework_destination}${lib}")
      endif()
    endforeach()
  endforeach()
endfunction()
