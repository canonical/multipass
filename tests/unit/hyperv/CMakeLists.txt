target_sources(multipass_cpp_tests
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/test_hyperv_backend.cpp
)

if(HYPERV_HCS_ENABLED)
  target_sources(multipass_cpp_tests PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/test_hyperv_disk_layout.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_hcs_ownership.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_migrating_hyperv_virtual_machine.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_migrating_hyperv_virtual_machine_factory.cpp)
endif()
