cmake_minimum_required(VERSION 3.19)

set(MP_VCPKG_DISABLE_CMAKE_TARGETS grpc_unsecure grpc++_unsecure grpc_authorization_provider grpc++_alts grpc++_reflection grpcpp_channelz grpc++_error_details)

function(do_target_postprocessing)
    get_property(DIR_TARGETS DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(TARGET_NAME IN LISTS DIR_TARGETS)
        if(TARGET_NAME IN_LIST MP_VCPKG_DISABLE_CMAKE_TARGETS)
            message(STATUS "${TARGET_NAME} is excluded from ALL")
            set_property(TARGET "${TARGET_NAME}" PROPERTY EXCLUDE_FROM_ALL TRUE)
        endif()
    endforeach()
endfunction()

cmake_language(DEFER CALL do_target_postprocessing)

# vcpkg conditionally overrides the install, so check that.
if(COMMAND _install)
    set(INSTALL_TO_HOOK _install)
else()
    set(INSTALL_TO_HOOK install)
endif()

function(${INSTALL_TO_HOOK})
    # Parse the TARGETS from install command
    cmake_parse_arguments(ARG "" "" "TARGETS" ${ARGN})

    foreach(target IN LISTS ARG_TARGETS)
        if(target IN_LIST MP_VCPKG_DISABLE_CMAKE_TARGETS)
            message(STATUS "Skipping install for disabled target: ${target}")
            return()
        endif()
    endforeach()

    cmake_language(CALL "_${INSTALL_TO_HOOK}" ${ARGN})
endfunction()

include(${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt.orig)