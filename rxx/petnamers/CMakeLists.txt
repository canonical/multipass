#target_include_directories(${crate_name} PUBLIC ${Qt6Core_INCLUDE_DIRS})
target_link_libraries(${crate_name} PRIVATE
  rust_logger
)
set_target_properties(${crate_name}_rs PROPERTIES
  INTERFACE_LINK_LIBRARIES "\$<LINK_ONLY:rust_logger>"
)
