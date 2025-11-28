#target_include_directories(${crate_name} PUBLIC ${Qt6Core_INCLUDE_DIRS})
target_link_libraries(${crate_name} PRIVATE
    logger
)
