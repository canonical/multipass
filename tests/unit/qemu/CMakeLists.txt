target_sources(multipass_tests
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/test_qemu_backend.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_qemu_mount_handler.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_qemu_snapshot.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_qemu_vm_process_spec.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_qemu_vmstate_process_spec.cpp
)

add_subdirectory(${MULTIPASS_PLATFORM})
