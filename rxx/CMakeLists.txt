#Find cargo, which will be a hard dependency
find_program(CARGO_EXECUTABLE cargo)

#Set build variables
if (cmake_build_type_lower MATCHES "release")
  set(rust_build "release")
  set(rust_build_flag "--release")
else()
  set(rust_build "debug")
  set(rust_build_flag "")
endif()
if (WIN32 AND (NOT MINGW))
  set(lib_prefix "")
  set(lib_suffix "lib")
else()
  set(lib_prefix "lib")
  set(lib_suffix "a")
endif()
# Build output path
set(RUST_BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/target/${rust_build}")
#Crates to build. Both consumers and providers of code to the C++ side
#have to be built, since the CXX glue code has to be generated and compiled
#for said usage
set(CRATES
  rust_logger
  petnamers
)

if(NOT CRATES STREQUAL "")
  # cxx.cc is compiled and linked into the static libraries by cargo automatically
  # There is no documentation about this but you can check the symbols by doing
  # nm -g mylib.a | grep string
  # This means that no need to handle cxx.cc
  add_library(cxx INTERFACE)
  target_include_directories(cxx INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/target/cxxbridge")
endif()
set(CARGO_PRODUCTS "")
foreach(crate_name ${CRATES})
  set(crate_path "${CMAKE_CURRENT_SOURCE_DIR}/${crate_name}")
  #Check that it is a crate (since target is not a crate)
  if (IS_DIRECTORY "${crate_path}" AND EXISTS "${crate_path}/Cargo.toml")
    message(STATUS "Configuring crate: ${crate_path}")
    #Static library path for the rust source.
    set(rust_out "${RUST_BINARY_DIR}/${lib_prefix}${crate_name}.${lib_suffix}")
    #Target to depend on cargo build.
    list(APPEND CARGO_PRODUCTS ${rust_out})
    add_library(${crate_name}_rs STATIC IMPORTED)
    set_target_properties(${crate_name}_rs PROPERTIES IMPORTED_LOCATION ${rust_out})

    set(bridge_source "${CMAKE_CURRENT_SOURCE_DIR}/target/cxxbridge/${crate_name}/src")
    set(bridge_cc "${bridge_source}/lib.rs.cc")

    list(APPEND CARGO_PRODUCTS ${bridge_cc})
    #Library creation to compile the CXX glue source
    add_library(${crate_name} STATIC ${bridge_cc})
    #Here we can place the crate-wise dependencies
    add_subdirectory(${crate_name})
    #The CXX glue staticlib must link the rust staticlib
    target_link_libraries(${crate_name} PUBLIC ${crate_name}_rs cxx)
    add_dependencies(${crate_name} ${crate_name}_rs)
  endif()
endforeach()

add_custom_command(
  OUTPUT ${CARGO_PRODUCTS}
  COMMAND ${CARGO_EXECUTABLE} build ${rust_build_flag} --lib
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Building Rust crates and generating C++ glue..."
)
