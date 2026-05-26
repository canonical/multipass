vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO soasis/out_ptr
    REF 02a577edfcf25e2519e380a95c16743b7e5878a1
    SHA512 9020712571a1200fde61e407dd536310e80f03e08436cd0754e3ee7b88601d248b5b42b2f90a23c7b40e4ac6e1722b94ccc4dfa52c6a26e468516c035f816221
    HEAD_REF main
)

# Header-only library; upstream CMakeLists.txt has install() calls commented out,
# so we install the headers and config manually.
file(INSTALL "${SOURCE_PATH}/include/ztd" DESTINATION "${CURRENT_PACKAGES_DIR}/include")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/out_ptr-config.cmake"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/out_ptr")

set(VCPKG_POLICY_EMPTY_INCLUDE_FOLDER disabled)