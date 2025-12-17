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

function(determine_version OUTPUT_VARIABLE)
  # use upstream repo as the authoritative reference when checking for release status
  # set -DMULTIPASS_UPSTREAM="" to use the local repository
  if(MULTIPASS_UPSTREAM)
    set(MULTIPASS_UPSTREAM "${MULTIPASS_UPSTREAM}/")
  endif()

  execute_process(COMMAND git describe --all --exact --match "${MULTIPASS_UPSTREAM}release/*"
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE GIT_RELEASE_BRANCH
                  OUTPUT_STRIP_TRAILING_WHITESPACE
                  ERROR_QUIET)

  execute_process(COMMAND git describe --long --abbrev=8
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE GIT_VERSION
                  OUTPUT_STRIP_TRAILING_WHITESPACE)

  # only use -rc tags on release/* branches
  string(REGEX MATCH "release/[0-9]+.[0-9]+" GIT_RELEASE_MATCH "${GIT_RELEASE_BRANCH}")
  if(GIT_RELEASE_MATCH)
      if(NOT DEFINED MULTIPASS_UPSTREAM)
        message(FATAL_ERROR "You need to set MULTIPASS_UPSTREAM for a release build.\
                             \nUse an empty string to make local the authoritative repository.")
      endif()

      execute_process(COMMAND git describe --exact
                      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                      OUTPUT_VARIABLE GIT_RELEASE
                      OUTPUT_STRIP_TRAILING_WHITESPACE
                      ERROR_QUIET)

      execute_process(COMMAND git describe --tags --match *-rc[0-9]* --abbrev=0
                      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                      OUTPUT_VARIABLE GIT_TAG
                      OUTPUT_STRIP_TRAILING_WHITESPACE)
  else()
      execute_process(COMMAND git describe --tags --match *-dev --abbrev=0
                      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                      OUTPUT_VARIABLE GIT_TAG
                      OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()

  string(REGEX MATCH "^v.+-([0-9]+)-(g.+)$" GIT_VERSION_MATCH ${GIT_VERSION})

  if(GIT_RELEASE)
    set(NEW_VERSION ${GIT_RELEASE})
  elseif(GIT_VERSION_MATCH AND MULTIPASS_BUILD_LABEL)
    set(NEW_VERSION ${GIT_TAG}.${CMAKE_MATCH_1}.${MULTIPASS_BUILD_LABEL}+${CMAKE_MATCH_2})
  elseif(GIT_VERSION_MATCH)
    set(NEW_VERSION ${GIT_TAG}.${CMAKE_MATCH_1}+${CMAKE_MATCH_2})
  else()
    message(FATAL_ERROR "Failed to parse version number: ${GIT_VERSION}")
  endif()

  # assert that the output of `git describe` does not include "full" any longer (after merger)
  string(FIND "${NEW_VERSION}" "full" FULL_POSITION)
  if(FULL_POSITION GREATER_EQUAL 0)
      message(FATAL_ERROR "Version string should not contain 'full': ${NEW_VERSION}")
  endif()

  if(APPLE OR WIN32)
    set(VERSION_SEPARATOR "+")
    string(FIND "${NEW_VERSION}" "+" PLUS_POSITION)
    if(PLUS_POSITION GREATER_EQUAL 0)
      set(VERSION_SEPARATOR ".")
    endif()

    if(APPLE)
      string(APPEND NEW_VERSION "${VERSION_SEPARATOR}mac")
    else()
      string(APPEND NEW_VERSION "${VERSION_SEPARATOR}win")
    endif()
  endif()

  string(REGEX MATCH "^v(.+)" VERSION_MATCH ${NEW_VERSION})

  if(VERSION_MATCH)
    set(${OUTPUT_VARIABLE} ${CMAKE_MATCH_1} PARENT_SCOPE)
  else()
    message(FATAL_ERROR "Invalid tag detected: ${NEW_VERSION}")
  endif()
endfunction()

function(determine_version_components VERSION_STRING SEMANTIC_VERSION BUILD_NUMBER)
  # stuff before + is the version and after the + is the build metadata
  string(FIND "${VERSION_STRING}" "+" PLUS_POS)
  string(SUBSTRING "${VERSION_STRING}" 0 ${PLUS_POS} MULTIPASS_SEMANTIC_VERSION)
  if (PLUS_POS GREATER -1)
      string(SUBSTRING "${VERSION_STRING}" ${PLUS_POS} -1 MULTIPASS_BUILD_STRING)
      # if the build metadata starts with a g, the hexadecimal chars after that are a commit hash
      # otherwise, we do not derive any build string
      string(REGEX MATCH "^[+]g([a-f0-9]+)" "" "${MULTIPASS_BUILD_STRING}")
      set(BUILD_NUMBER_HEX ${CMAKE_MATCH_1})
  endif()
  # convert the hexadecimal commit hash to decimal. if none was extracted, this defaults to 0
  # we need it to be decimal because flutter --build-number accepts only decimal chars
  math(EXPR MULTIPASS_BUILD_NUMBER "0x0${BUILD_NUMBER_HEX}")

  set(${SEMANTIC_VERSION} ${MULTIPASS_SEMANTIC_VERSION} PARENT_SCOPE)
  set(${BUILD_NUMBER} ${MULTIPASS_BUILD_NUMBER} PARENT_SCOPE)
endfunction()
