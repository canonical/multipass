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
# Authored by: Antoni Bertolin Monferrer <antoni.monferrer@canonical.com>

#Set build variables
if (cmake_build_type_lower MATCHES "release")
  set(rust_build "release")
  set(rust_build_flag "--release")
else()
  set(rust_build "debug")
  set(rust_build_flag "")
endif()
if (WIN32)
  set(lib_prefix "")
  set(lib_suffix "lib")
else()
  set(lib_prefix "lib")
  set(lib_suffix "a")
endif()

# Build output path
set(RUST_BUILD_DIR "${CMAKE_BINARY_DIR}/rxx")
set(RUST_BIN_DIR "${RUST_BUILD_DIR}/${rust_build}")

#Find cargo, which will be a hard dependency
find_program(CARGO_EXECUTABLE cargo)
#Set the build directory via env variable
set(CARGO_CMD ${CMAKE_COMMAND} -E env "CARGO_TARGET_DIR=${RUST_BUILD_DIR}" ${CARGO_EXECUTABLE})
#Crates to build. Both consumers and providers of code to the C++ side
#have to be built, since the CXX glue code has to be generated and compiled
#for said usage
set(CRATES
  petname
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
set(RUST_DEPENDENCIES "")
foreach(crate_name ${CRATES})
  set(crate_path "${CMAKE_CURRENT_SOURCE_DIR}/${crate_name}")
  #Check that it is a crate (since target is not a crate)
  if (IS_DIRECTORY "${crate_path}" AND EXISTS "${crate_path}/Cargo.toml")
    message(STATUS "Configuring crate: ${crate_path}")
    #Static library path for the rust source.
    set(rust_out "${RUST_BIN_DIR}/${lib_prefix}${crate_name}.${lib_suffix}")
    #Target to depend on cargo build.
    add_library(${crate_name}_rs STATIC IMPORTED)
    set_target_properties(${crate_name}_rs PROPERTIES IMPORTED_LOCATION ${rust_out})
    set(${crate_name}_link_libs "")
    if (WIN32)
      list(APPEND ${crate_name}_link_libs "$<LINK_ONLY:ntdll>")
    endif()
    set(bridge_source "${RUST_BUILD_DIR}/cxxbridge/${crate_name}/src")
    set(bridge_cc "${bridge_source}/lib.rs.cc")

    #Library creation to compile the CXX glue source
    add_library(${crate_name} STATIC ${bridge_cc})
    #Here we can place the crate-wise dependencies
    add_subdirectory(${crate_name})
    #The CXX glue staticlib must link the rust staticlib
    target_link_libraries(${crate_name} PUBLIC ${crate_name}_rs cxx)
    target_link_libraries(${crate_name}_rs INTERFACE ${${crate_name}_link_libs})
    add_dependencies(${crate_name} ${crate_name}_rs)

    list(APPEND RUST_DEPENDENCIES ${bridge_cc})
    list(APPEND RUST_DEPENDENCIES ${rust_out})
  endif()
endforeach()

list(JOIN ACTIVE_FEATURES  "," FEATURES_CSV)
add_custom_command(
  OUTPUT ${RUST_DEPENDENCIES}
  COMMAND ${CARGO_CMD} build
  --manifest-path ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml
  --workspace
  ${rust_build_flag}
  --lib
  --features "${FEATURES_CSV}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Building Rust crates and generating C++ glue..."
)
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

if(cmake_build_type_lower MATCHES "coverage")
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
