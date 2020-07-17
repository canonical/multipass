target_sources(multipass_tests
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/fake_libvirt_library.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_libvirt_backend.cpp
)

add_library(broken_libvirt SHARED
    ${CMAKE_CURRENT_LIST_DIR}/broken_libvirt_library.cpp)

target_link_options(multipass_tests PRIVATE -rdynamic)

target_compile_definitions(libvirt_backend_test PRIVATE
    ${c_mock_defines})

target_link_libraries(multipass_tests libvirt_backend_test)
