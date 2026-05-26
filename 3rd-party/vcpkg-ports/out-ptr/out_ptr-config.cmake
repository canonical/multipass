if(TARGET ztd::out_ptr)
    return()
endif()

add_library(ztd::out_ptr IMPORTED INTERFACE)
set_target_properties(ztd::out_ptr PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../../include"
)