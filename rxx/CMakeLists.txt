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

#Set build variables
if (CMAKE_BUILD_TYPE_LOWER MATCHES "release")
  set(RUST_BUILD "release")
  set(RUST_BUILD_FLAG "--release")
else()
  set(RUST_BUILD "debug")
  set(RUST_BUILD_FLAG "")
endif()
if (WIN32)
  set(LIB_PREFIX "")
  set(LIB_SUFFIX "lib")
else()
  set(LIB_PREFIX "lib")
  set(LIB_SUFFIX "a")
endif()

# Build output path
set(RUST_BUILD_DIR "${CMAKE_BINARY_DIR}/rxx")
set(RUST_BIN_DIR "${RUST_BUILD_DIR}/${RUST_BUILD}")

#Find cargo, which will be a hard dependency
find_program(CARGO_EXECUTABLE cargo)
#Set the build directory via env variable
set(CARGO_CMD ${CMAKE_COMMAND} -E env "CARGO_TARGET_DIR=${RUST_BUILD_DIR}" ${CARGO_EXECUTABLE})
#Crates to build. Both consumers and providers of code to the C++ side
#have to be built, since the CXX glue code has to be generated and compiled
#for said usage
set(CRATES
  namegen
)

if(NOT CRATES STREQUAL "")
  # cxx.cc is compiled and linked into the static libraries by cargo automatically
  # There is no documentation about this but you can check the symbols by doing
  # nm -g mylib.a | grep string
  # This means that no need to handle cxx.cc
  add_library(cxx INTERFACE)
  target_include_directories(cxx INTERFACE "${RUST_BUILD_DIR}/cxxbridge")
endif()

set(ACTIVE_FEATURES "default")
set(CARGO_GENERATED_FILES "")
foreach(CRATE_NAME ${CRATES})
  set(CRATE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${CRATE_NAME}")
  #Check that it is a crate (since target is not a crate)
  if (IS_DIRECTORY "${CRATE_PATH}" AND EXISTS "${CRATE_PATH}/Cargo.toml")
    message(STATUS "Configuring crate: ${CRATE_PATH}")
    #Static library path for the rust source.
    set(RUST_OUT "${RUST_BIN_DIR}/${LIB_PREFIX}${CRATE_NAME}.${LIB_SUFFIX}")
    #Target to depend on cargo build.
    add_library(${CRATE_NAME}_rs STATIC IMPORTED)
    set_target_properties(${CRATE_NAME}_rs PROPERTIES IMPORTED_LOCATION ${RUST_OUT})
    set(${CRATE_NAME}_LINK_LIBS "")
    if (WIN32)
      list(APPEND ${CRATE_NAME}_LINK_LIBS "$<LINK_ONLY:ntdll>")
    endif()
    set(BRIDGE_SOURCE "${RUST_BUILD_DIR}/cxxbridge/${CRATE_NAME}/src")
    set(BRIDGE_CC "${BRIDGE_SOURCE}/lib.rs.cc")

    list(APPEND CARGO_GENERATED_FILES "${RUST_OUT}" "${BRIDGE_CC}")
    set_source_files_properties(${BRIDGE_CC} PROPERTIES GENERATED TRUE)
    set_source_files_properties(${RUST_OUT} PROPERTIES GENERATED TRUE)

    #Library creation to compile the CXX glue source
    add_library(${CRATE_NAME} STATIC ${BRIDGE_CC})
    #Here we can place the crate-wise dependencies
    add_subdirectory(${CRATE_NAME})
    #The CXX glue staticlib must link the rust staticlib
    target_link_libraries(${CRATE_NAME} PUBLIC ${CRATE_NAME}_rs cxx)
    target_link_libraries(${CRATE_NAME}_rs INTERFACE ${${CRATE_NAME}_LINK_LIBS})
  endif()
endforeach()

list(JOIN ACTIVE_FEATURES  "," FEATURES_CSV)
add_custom_command(
  OUTPUT ${CARGO_GENERATED_FILES}
  COMMAND ${CARGO_CMD} build
  --manifest-path ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml
  --workspace
  ${RUST_BUILD_FLAG}
  --lib
  --features "${FEATURES_CSV}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Building Rust crates and generating C++ glue..."
)

add_custom_target(cargo_build DEPENDS ${CARGO_GENERATED_FILES})

foreach(CRATE_NAME ${CRATES})
    if(TARGET ${CRATE_NAME})
        add_dependencies(${CRATE_NAME} cargo_build)
        add_dependencies(${CRATE_NAME}_rs cargo_build)
    endif()
endforeach()
#The macros crate cannot contain tests
set(TESTING_EXCLUDE_CRATES_ARG --exclude macros)
add_test(
  NAME      multipass_rust_tests
  COMMAND   ${CARGO_CMD} test
  --manifest-path ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml
  --no-fail-fast
  --workspace
  --lib
  --tests
  --features "${FEATURES_CSV}"
  ${TESTING_EXCLUDE_CRATES_ARG}
)
set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${RUST_BIN_DIR}")

if(CMAKE_BUILD_TYPE_LOWER MATCHES "coverage")
  execute_process(
    COMMAND ${CARGO_CMD} tarpaulin --version
    RESULT_VARIABLE TARPAULIN_INSTALLED
    OUTPUT_VARIABLE TARPAULIN_VERSION
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  if (NOT TARPAULIN_INSTALLED EQUAL 0)
    execute_process(
      COMMAND ${CARGO_CMD} install cargo-tarpaulin
    )
  endif()
    #Engine is for non-x86_64 processors. Can only use if return codes from test are always 0 + no fork or similar syscalls
    add_custom_target(rust_coverage_tarpaulin
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND ${CARGO_CMD} tarpaulin
        --out Lcov
        --output-dir ${CMAKE_BINARY_DIR}
        --exclude-files '../*'
        --engine llvm
        --workspace
        ${TESTING_EXCLUDE_CRATES_ARG}
      COMMENT "Running Rust coverage with Tarpaulin"
    )
endif()
