# Mock qemu-img executable for QEMU tests
add_executable(qemu-img
  mock_qemuimg.cpp)

set_target_properties(qemu-img
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/mocks"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/mocks"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/mocks")

add_dependencies(multipass_qemu_tests
  qemu-img
)

add_subdirectory(${MULTIPASS_PLATFORM})
