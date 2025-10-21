set(WRAP_DIR "${CURRENT_BUILDTREES_DIR}/src-wrap")
file(MAKE_DIRECTORY "${WRAP_DIR}")

set(WRAP_CMAKE  [=[
cmake_minimum_required(VERSION 3.20)
project(grpc_trimmed LANGUAGES C CXX)

function(_internal_install)
    _install(${ARGN})
endfunction()

# define the ones you want to skip
set(TARGETS_TO_SKIP
  grpc_unsecure
  grpc++_unsecure
  grpc_authorization_provider
  grpc_csharp_plugin
  grpc_node_plugin
  grpc_objective_c_plugin
  grpc_php_plugin
  grpc_python_plugin
  grpc_ruby_plugin
  grpc++_alts
  grpc++_reflection
)

# Override install to filter by target
function(install)
    # Parse the TARGETS from install command
    cmake_parse_arguments(ARG "" "" "TARGETS" ${ARGN})
    
    if(ARG_TARGETS)
        foreach(target ${ARG_TARGETS})
            if(target IN_LIST TARGETS_TO_SKIP)
                # If we get here, skip this install command
                message(STATUS "Skipping install for: ${ARG_TARGETS}")
                return()
            endif()
        endforeach()
        _internal_install(${ARGN})
    else()
        _internal_install(${ARGN})
    endif()
endfunction()

# Bring in upstream as a subdir (no install, nothing built by default)
add_subdirectory("@SOURCE_PATH@" ${CMAKE_BINARY_DIR}/grpc)

# Collect every upstream target, exclude them from ALL
get_property(_all_tgts DIRECTORY "@SOURCE_PATH@" PROPERTY BUILDSYSTEM_TARGETS)
foreach(t IN LISTS _all_tgts)
    message(STATUS "TARGET: ${t}")
    if(t IN_LIST TARGETS_TO_SKIP)
        message(STATUS "Excluding target ${t} from ALL")
        set_property(TARGET "${t}" PROPERTY EXCLUDE_FROM_ALL TRUE)
    endif()
endforeach()
]=])

string(CONFIGURE "${WRAP_CMAKE}" WRAP_CMAKE_ESCAPED @ONLY)

# --- Write wrapper CMakeLists.txt ---
file(WRITE "${WRAP_DIR}/CMakeLists.txt" "${WRAP_CMAKE_ESCAPED}")