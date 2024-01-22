target_sources(multipass_tests
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/test_qemu_backend.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_qemu_img_utils.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_qemu_mount_handler.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_qemu_snapshot.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_qemu_vm_process_spec.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_qemu_vmstate_process_spec.cpp
)

add_executable(qemu-img
  mock_qemuimg.cpp)

set_target_properties(qemu-img
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/mocks"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/mocks"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/mocks")

add_dependencies(multipass_tests
  qemu-img
)

add_subdirectory(${MULTIPASS_PLATFORM})
