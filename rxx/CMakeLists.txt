find_program(CARGO_EXECUTABLE cargo)
if (cmake_build_type_lower MATCHES "release")
  set(rust_build "release")
  set(rust_build_flag "--release")
else()
  set(rust_build "debug")
  set(rust_build_flag "")
endif()
get_filename_component(DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
execute_process(
  COMMAND cxxbridge --version
  RESULT_VARIABLE check_result
  OUTPUT_VARIABLE check_output
  ERROR_VARIABLE check_error
)
if(check_result EQUAL 0)
  execute_process(
    COMMAND cargo install cxxbridge-cmd
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

set(RUST_GENSRC_DIR "${MULTIPASS_GENERATED_SOURCE_DIR}/multipass/${DIR_NAME}")
file(MAKE_DIRECTORY ${RUST_GENSRC_DIR})

set(CRATES
  rust_logger
  petnamers
)
if(NOT CRATES STREQUAL "")
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
set(STATICLIBS "")
foreach(crate_name ${CRATES})
  set(crate_path "${CMAKE_CURRENT_SOURCE_DIR}/${crate_name}")
  if (IS_DIRECTORY "${crate_path}" AND EXISTS "${crate_path}/Cargo.toml")
    message(STATUS "Configuring crate: ${crate_path}")
    set(rust_out "${CMAKE_CURRENT_SOURCE_DIR}/target/${rust_build}/lib${crate_name}.a")
    add_library(${crate_name}_rs STATIC IMPORTED)
    set_target_properties(${crate_name}_rs PROPERTIES IMPORTED_LOCATION ${rust_out})
    list(APPEND STATICLIBS ${crate_name}_rs)
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
    #cxx.cc is compiled and linked into the static libraries by cargo automatically
    # There is not documentation about this but you can check the symbols by doing
    # nm -g mylib.a | grep string
    # This means that no need to handle cxx.cc

    add_library(${crate_name} STATIC ${bridge_cc})
    add_subdirectory(${crate_name})
    #target_include_directories(rust_ffi PUBLIC ${CMAKE_CURRENT_BINARY_DIR} "${crate_path}/include")
    target_link_libraries(${crate_name} PUBLIC ${crate_name}_rs)
    add_dependencies(${crate_name} ${crate_name}_rs)
  endif()
endforeach()
add_custom_command(
        OUTPUT ${STATICLIBS}
        COMMAND cargo build ${rust_build_flag} --lib
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        BYPRODUCTS ${STATICLIBS}
        COMMENT "Building crates..."
    )
