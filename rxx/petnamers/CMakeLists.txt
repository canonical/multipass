#target_include_directories(${crate_name} PUBLIC ${Qt6Core_INCLUDE_DIRS})
#Link dependency of the bridge code
#target_link_libraries(${crate_name} PRIVATE
#  rust_logger
#)
# Here the rust build depends on the C++ side of the rust_logger bridge.
# Because we did not build it, we specify the dependency like:
set_target_properties(${crate_name}_rs PROPERTIES
  INTERFACE_LINK_LIBRARIES "\$<LINK_ONLY:rust_logger>"
)
