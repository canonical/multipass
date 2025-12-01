#Find cargo, which will be a hard dependency
find_program(CARGO_EXECUTABLE cargo)
#Set build variables
if (WIN32)
  set(CMAKE_DIR_COPY ${CMAKE_COMMAND} -E copy_directory)
  set(CMAKE_FILE_COPY ${CMAKE_COMMAND} -E copy)
else()
  set(CMAKE_DIR_COPY  "cp" "-rL")
  set(CMAKE_FILE_COPY "cp" "-L")
endif()

if (cmake_build_type_lower MATCHES "release")
  set(rust_build "release")
  set(rust_build_flag "--release")
  set(strip_command "strip" "-d")
else()
  set(rust_build "debug")
  set(rust_build_flag "")
  set(strip_command "touch")
endif()
set(CXX_BRIDGE_CMD "cargo" "run" "--bin" "cxxbridge" "--")
#So that paths are agnostic to directory name
get_filename_component(DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
#We need cxxbridge-cmd to be installed for CXX glue code generation.
#execute_process(
#  COMMAND cxxbridge --version
#  RESULT_VARIABLE check_result
#  OUTPUT_VARIABLE check_output
#  ERROR_VARIABLE check_error
#)
#if(check_result EQUAL 0)
#  #If it is not installed, we install it via cargo (convenient)
#  execute_process(
#    COMMAND ${CARGO_EXECUTABLE} install cxxbridge-cmd --version ^1.0
#    RESULT_VARIABLE install_result
#    OUTPUT_VARIABLE install_output
#    ERROR_VARIABLE install_error
#    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#  )
##Check if installation was successful
#  if(NOT install_result EQUAL 0)
#    message(FATAL_ERROR "Failed to install cxxbridge-cmd: ${install_error}")
#  else()
#    message(STATUS "cxxbridge-cmd installed successfully")
#  endif()
#endif()
#Directory where the headers and source of CXX glue will be placed
set(RUST_GENSRC_DIR "${MULTIPASS_GENERATED_SOURCE_DIR}/multipass/${DIR_NAME}")
file(MAKE_DIRECTORY ${RUST_GENSRC_DIR})
set(RUST_BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/target/${rust_build}")
#Crates to build. Both consumers and providers of code to the C++ side
#have to be built, since the CXX glue code has to be generated and compiled
#for said consumption
set(CRATES
  rust_logger
  petnamers
)

#All libraries are built simultaneously thanks to the Rust workspace
add_custom_target(build_rust_libs
    COMMAND ${CARGO_EXECUTABLE} build ${rust_build_flag} --lib
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building Rust crates..."
)

if(NOT CRATES STREQUAL "")
  #Generate the CXX header. It contains the CXX type bindings to use Rust
  #native types in C++. The source will be compiled into each static library
  #that contains the CXX bridge.
  # cxx.cc is compiled and linked into the static libraries by cargo automatically
  # There is no documentation about this but you can check the symbols by doing
  # nm -g mylib.a | grep string
  # This means that no need to handle cxx.cc
  #set(CXX_HEADER "${RUST_GENSRC_DIR}/cxx.h")
  #execute_process(
  #  COMMAND ${CXX_BRIDGE_CMD} --header -o ${CXX_HEADER}
  #  RESULT_VARIABLE check_result
  #  OUTPUT_VARIABLE check_output
  #  ERROR_VARIABLE check_error
  #  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  #  OUTPUT_STRIP_TRAILING_WHITESPACE
  #)
  file(MAKE_DIRECTORY "${MULTIPASS_GENERATED_SOURCE_DIR}/rxx")
  add_custom_target(copy_header
    COMMAND ${CMAKE_FILE_COPY} "${CMAKE_CURRENT_SOURCE_DIR}/target/cxxbridge/rust/cxx.h" "${MULTIPASS_GENERATED_SOURCE_DIR}/rxx/cxx.h"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS build_rust_libs
  )

endif()

foreach(crate_name ${CRATES})
  set(crate_path "${CMAKE_CURRENT_SOURCE_DIR}/${crate_name}")
  #Check that it is a crate (since target is not a crate)
  if (IS_DIRECTORY "${crate_path}" AND EXISTS "${crate_path}/Cargo.toml")
    message(STATUS "Configuring crate: ${crate_path}")
    #Static library path for the rust source.
    set(rust_out "${RUST_BINARY_DIR}/lib${crate_name}.a")
    #Target to depend on cargo build.
    add_library(${crate_name}_rs STATIC IMPORTED)
    set_target_properties(${crate_name}_rs PROPERTIES IMPORTED_LOCATION ${rust_out})
    add_dependencies(${crate_name}_rs build_rust_libs)

    #CXX glue generation
    #The strip command will remove all existing debuginfo from the rust staticlib
    #only in release builds, the rest will contain debuginfo
    #set(crate_rs "${crate_path}/src/lib.rs")
    set(bridge_source "${CMAKE_CURRENT_SOURCE_DIR}/target/cxxbridge/${crate_name}/src")
    set(bridge_header "${bridge_source}/lib.rs.h")
    set(bridge_cc "${bridge_source}/lib.rs.cc")
    #add_custom_command(
    #    OUTPUT ${bridge_h} ${bridge_cc}
    #    COMMAND ${CXX_BRIDGE_CMD} "${crate_rs}" --header --output "${bridge_h}"
    #    COMMAND ${CXX_BRIDGE_CMD} "${crate_rs}" --output "${bridge_cc}"
    #    #COMMAND ${strip_command} "${rust_out}"
    #    DEPENDS "${crate_rs}" "${CXX_HEADER}"
    #    COMMENT "Generating cxxbridge glue for ${crate_name}"
    #)
    add_custom_target(
      ${crate_name}_h
      COMMAND ${CMAKE_FILE_COPY} "${bridge_header}" "${RUST_GENSRC_DIR}/${crate_name}.h"
      DEPENDS build_rust_libs
    )

    #Library creation to compile the CXX glue source
    add_library(${crate_name} STATIC ${bridge_cc})
    #Here we can place the crate-wise dependencies
    add_subdirectory(${crate_name})
    #The CXX glue staticlib must link the rust staticlib
    target_link_libraries(${crate_name} PUBLIC ${crate_name}_rs)
    add_dependencies(${crate_name} ${crate_name}_rs build_rust_libs copy_header ${crate_name}_h)
  endif()
endforeach()
