target_sources(multipass_tests
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/test_apparmored_process.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_backend_utils.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_local_network_access_manager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_platform_linux.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_snap_utils.cpp
)

add_executable(apparmor_parser
  mock_apparmor_parser.cpp)

set_target_properties(apparmor_parser
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/mocks"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/mocks"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/mocks")

add_dependencies(multipass_tests apparmor_parser)
