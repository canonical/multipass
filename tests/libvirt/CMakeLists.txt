target_sources(multipass_tests
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/mock_libvirt.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_libvirt_backend.cpp
)

target_compile_definitions(libvirt_backend_test PRIVATE
    ${c_mock_defines})

target_link_libraries(multipass_tests libvirt_backend_test)

