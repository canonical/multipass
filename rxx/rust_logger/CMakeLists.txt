#target_include_directories(${crate_name} PUBLIC ${Qt6Core_INCLUDE_DIRS})
target_link_libraries(${crate_name} PUBLIC
    logger
)
#target_link_libraries(${crate_name} PUBLIC fmt::fmt-header-only Qt::Core)
