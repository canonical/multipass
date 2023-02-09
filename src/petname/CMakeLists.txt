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
#
# Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>

set(PETNAME_GENERATED_SOURCE_DIR ${MULTIPASS_GENERATED_SOURCE_DIR}/multipass/petname)
set(PETNAME_GENERATED_HEADER ${PETNAME_GENERATED_SOURCE_DIR}/names.h)
file(MAKE_DIRECTORY ${PETNAME_GENERATED_SOURCE_DIR})

add_executable(text_to_string_array
  text_to_string_array.cpp)

add_custom_command(
  OUTPUT "${PETNAME_GENERATED_HEADER}"
  COMMAND $<TARGET_FILE:text_to_string_array>
  ARGS ${CMAKE_CURRENT_SOURCE_DIR}/adjectives.txt ${CMAKE_CURRENT_SOURCE_DIR}/adverbs.txt
       ${CMAKE_CURRENT_SOURCE_DIR}/names.txt ${PETNAME_GENERATED_HEADER}
  DEPENDS text_to_string_array
  COMMENT "Converting petnames to c++ header"
  VERBATIM)

set_source_files_properties(${PETNAME_GENERATED_HEADER} PROPERTIES GENERATED TRUE)

add_library(petname STATIC
  ${PETNAME_GENERATED_HEADER}
  petname.cpp
  make_name_generator.cpp)
