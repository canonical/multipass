# Here the rust build depends on the C++ side of the rust_logger bridge because
# the rust_logger crate is compiled within this crate's static library.
# Because we did not build it, we specify the dependency like:
set_target_properties(${crate_name}_rs PROPERTIES
  INTERFACE_LINK_LIBRARIES "\$<LINK_ONLY:rust_logger>"
)
#Add necessary sources for wrappers (it is advised that the source is named after
# the header in include/multipass/)
target_sources(${crate_name} PRIVATE "src/name_generator_rs.cpp")
#Link necessary libraries for the exceptions header
target_link_libraries(${crate_name} PRIVATE fmt::fmt-header-only Qt::Core)
