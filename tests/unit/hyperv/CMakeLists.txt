target_sources(multipass_cpp_tests
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/test_hyperv_backend.cpp
)

if(HYPERV_HCS_ENABLED)
  target_sources(multipass_cpp_tests PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/test_hyperv_disk_layout.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_hyperv_migration_service.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_hyperv_target_transaction.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_hcs_ownership.cpp)
endif()
