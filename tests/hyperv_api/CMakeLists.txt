
if(WIN32)
    target_sources(multipass_tests
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/test_it_hyperv_hcn_api.cpp
    )

    target_sources(multipass_tests
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/test_ut_hyperv_hcn_api.cpp
    )

    target_sources(multipass_tests
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/test_it_hyperv_hcs_api.cpp
    )
endif()

