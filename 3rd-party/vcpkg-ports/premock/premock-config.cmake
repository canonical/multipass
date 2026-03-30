if(TARGET premock::premock)
    return()
endif()

add_library(premock::premock INTERFACE IMPORTED)
set_target_properties(premock::premock PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../../include"
)