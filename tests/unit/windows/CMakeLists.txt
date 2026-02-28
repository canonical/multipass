target_sources(multipass_cpp_tests
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/powershell_test_helper.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_platform_win.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_powershell.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_smb_mount_handler.cpp
)
