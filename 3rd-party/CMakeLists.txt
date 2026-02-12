set(BUILD_SHARED_LIBS OFF)

if (MSVC)
  add_compile_options(-wd5045) #Disable warning about Spectre mitigation
endif()

function(add_subdirectory_compat)
    #
    # This is for CMake 4.0 compatibility.
    #
    # CMake 4.0 drops support CMake policies below 3.5 and some of our dependencies
    # declare their minimum required version as 3.4, and CMake 4.0 is not happy
    # with that. The CMAKE_POLICY_VERSION_MINIMUM option allows us to override that
    # without having to touch the dependency's source code.
    #
    # https://cmake.org/cmake/help/latest/variable/CMAKE_POLICY_VERSION_MINIMUM.html
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    add_subdirectory(${ARGV} SYSTEM)
endfunction()

add_library(gRPC INTERFACE)

target_link_libraries(gRPC INTERFACE
  gRPC::grpc++
  protobuf::libprotobuf)

if (NOT MSVC)
  target_compile_options(gRPC INTERFACE "-Wno-unused-parameter" "-Wno-non-virtual-dtor" "-Wno-pedantic")
endif ()

include(ExternalProject)

# QEMU itself
if (APPLE AND "qemu" IN_LIST MULTIPASS_BACKENDS)
    add_subdirectory(qemu SYSTEM)
endif ()

if(MSVC)
  set(JSONCPP_WITH_PKGCONFIG_SUPPORT OFF CACHE BOOL "Disable pkgconfig in jsoncpp" FORCE)
  set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF CACHE BOOL "Disable unit tests in jsoncpp" FORCE)
  set(JSONCPP_WITH_TESTS OFF CACHE BOOL "Disable test executables in jsoncpp lib" FORCE)
  add_subdirectory(jsoncpp SYSTEM)
endif()

# premock header only library to mock c-functions
add_library(premock INTERFACE)
target_include_directories(premock INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/premock)

# xz-decoder config
add_subdirectory(xz-decoder SYSTEM EXCLUDE_FROM_ALL)
