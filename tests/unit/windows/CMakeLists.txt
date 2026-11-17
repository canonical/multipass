target_sources(multipass_cpp_tests
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/powershell_test_helper.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_native_path.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_nrpt_rule.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_platform_win.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_powershell.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_smb_mount_handler.cpp
)

# These need administrative privileges, as they change the host's configuration.
target_sources(multipass_integration_tests
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/test_it_nrpt_rule.cpp
)

target_link_libraries(multipass_integration_tests PRIVATE shared_win)
