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
#So that paths are agnostic to directory name
get_filename_component(DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
#We need cxxbridge-cmd to be installed for CXX glue code generation.
execute_process(
  COMMAND cxxbridge --version
  RESULT_VARIABLE check_result
  OUTPUT_VARIABLE check_output
  ERROR_VARIABLE check_error
)
if(check_result EQUAL 0)
  #If it is not installed, we install it via cargo (convenient)
  execute_process(
    COMMAND ${CARGO_EXECUTABLE} install cxxbridge-cmd
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
#Check if installation was successful
  if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "Failed to install cxxbridge-cmd: ${install_error}")
  else()
    message(STATUS "cxxbridge-cmd installed successfully")
  endif()
endif()

#Directory where the headers and source of CXX glue will be placed
set(RUST_GENSRC_DIR "${MULTIPASS_GENERATED_SOURCE_DIR}/multipass/${DIR_NAME}")
file(MAKE_DIRECTORY ${RUST_GENSRC_DIR})

#Crates to build. Both consumers and providers of code to the C++ side
#have to be built, since the CXX glue code has to be generated and compiled
#for said consumption
set(CRATES
  rust_logger
  petnamers
)
if(NOT CRATES STREQUAL "")
  #Generate the CXX header. It contains the CXX type bindings to use Rust
  #native types in C++. The source will be compiled into each static library
  #that contains the CXX bridge.
  # cxx.cc is compiled and linked into the static libraries by cargo automatically
  # There is no documentation about this but you can check the symbols by doing
  # nm -g mylib.a | grep string
  # This means that no need to handle cxx.cc
  set(CXX_HEADER "${RUST_GENSRC_DIR}/cxx.h")
  execute_process(
    COMMAND cxxbridge --header -o ${CXX_HEADER}
    RESULT_VARIABLE check_result
    OUTPUT_VARIABLE check_output
    ERROR_VARIABLE check_error
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
endif()

#All libraries are built simultaneously thanks to the Rust workspace
add_custom_target(build_rust_libs
    COMMAND ${CARGO_EXECUTABLE} build ${rust_build_flag} --lib
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building Rust crates..."
)

foreach(crate_name ${CRATES})
  set(crate_path "${CMAKE_CURRENT_SOURCE_DIR}/${crate_name}")
  #Check that it is a crate (since target is not a crate)
  if (IS_DIRECTORY "${crate_path}" AND EXISTS "${crate_path}/Cargo.toml")
    message(STATUS "Configuring crate: ${crate_path}")
    #Static library path for the rust source.
    set(rust_out "${CMAKE_CURRENT_SOURCE_DIR}/target/${rust_build}/lib${crate_name}.a")
    #Target to depend on cargo build.
    add_library(${crate_name}_rs STATIC IMPORTED)
    set_target_properties(${crate_name}_rs PROPERTIES IMPORTED_LOCATION ${rust_out})
    add_dependencies(${crate_name}_rs build_rust_libs)

    #CXX glue generation
    set(crate_rs "${crate_path}/src/lib.rs")
    set(bridge_h "${RUST_GENSRC_DIR}/${crate_name}.h")
    set(bridge_cc "${RUST_GENSRC_DIR}/${crate_name}.cc")
    add_custom_command(
        OUTPUT ${bridge_h} ${bridge_cc}
        COMMAND cxxbridge "${crate_rs}" --header --output "${bridge_h}"
        COMMAND cxxbridge "${crate_rs}" --output "${bridge_cc}"
        DEPENDS "${crate_rs}" "${CXX_HEADER}"
        COMMENT "Generating cxxbridge glue for ${crate_name}"
    )


    #Library creation to compile the CXX glue source
    add_library(${crate_name} STATIC ${bridge_cc})
    #Here we can place the crate-wise dependencies
    add_subdirectory(${crate_name})
    #The CXX glue staticlib must link the rust staticlib
    target_link_libraries(${crate_name} PUBLIC ${crate_name}_rs)
    add_dependencies(${crate_name} ${crate_name}_rs)
  endif()
endforeach()
