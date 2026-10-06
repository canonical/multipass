target_sources(multipass_cpp_tests PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/test_hyperv_disk_layout.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_hyperv_network_translation.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_hyperv_target_transaction.cpp)

target_link_libraries(multipass_cpp_tests hyperv_migration)
