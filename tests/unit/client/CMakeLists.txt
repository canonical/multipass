target_sources(multipass_cpp_tests
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/test_autocompleter.cpp
  ${CMAKE_CURRENT_LIST_DIR}/cmd/test_autocomplete_from_rpc_provider.cpp
)
