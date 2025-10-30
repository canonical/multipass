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

# On-demand vcpkg function overriding
function(override_vcpkg_function)

    set(_add_keywords)

    foreach(arg ${ARGV})
        if(arg MATCHES "^ADD_")
            list(APPEND _add_keywords "${arg}")
        endif()
    endforeach()

    cmake_parse_arguments("OVERRIDE_VCPKG_FUNCTION"
        ""
        "FUNCTION_NAME"
        "${_add_keywords};REMOVE_LINES"
        ${ARGV}
    )

    set(args_to_add)

    foreach(add_keyword ${_add_keywords})
        # Strip the ADD_ prefix to get the target keyword
        string(REGEX REPLACE "^ADD_" "" target_keyword "${add_keyword}")
        set(_sublist "${target_keyword}")
        foreach(add_keyword_arg ${OVERRIDE_VCPKG_FUNCTION_${add_keyword}})
            # use | as delimiter
            string(APPEND _sublist "|${add_keyword_arg}")
        endforeach()
        list(APPEND args_to_add "${_sublist}")
    endforeach()

    set(_CODE [=[
        function(@OVERRIDE_VCPKG_FUNCTION_FUNCTION_NAME@)
            set(ARGN_MODIFIED ${ARGN})
            set(EXTRA_OPTIONS "@OVERRIDE_VCPKG_FUNCTION_ADD_OPTIONS@")
            set(ARGS_TO_ADD_ENCODED "@args_to_add@")
            set(LINES_TO_REMOVE "@OVERRIDE_VCPKG_FUNCTION_REMOVE_LINES@")
            separate_arguments(LINES_TO_REMOVE)

            # Process each encoded insertion
            foreach(_encoded ${ARGS_TO_ADD_ENCODED})
                # Decode the sublist
                string(REPLACE "|" ";" _sublist "${_encoded}")

                # Extract keyword and args
                list(POP_FRONT _sublist keyword)
                set(args_to_insert ${_sublist})

                # Find and insert
                list(FIND ARGN_MODIFIED "${keyword}" keyword_index)
                if(NOT keyword_index EQUAL -1)
                    math(EXPR insert_pos "${keyword_index} + 1")
                    list(INSERT ARGN_MODIFIED ${insert_pos} ${args_to_insert})
                endif()
            endforeach()

            # Remove lines verbatim
            foreach(line ${LINES_TO_REMOVE})
                list(REMOVE_ITEM ARGN_MODIFIED ${line})
            endforeach()

            # message(STATUS "@OVERRIDE_VCPKG_FUNCTION_FUNCTION_NAME@ hook:\nbefore:${ARGN}\n after: ${ARGN_MODIFIED}")
            _@OVERRIDE_VCPKG_FUNCTION_FUNCTION_NAME@(${ARGN_MODIFIED})
        endfunction()
    ]=])

    string(CONFIGURE "${_CODE}" _CODE_CONFIGURED @ONLY)
    cmake_language(EVAL CODE "${_CODE_CONFIGURED}")
endfunction()
