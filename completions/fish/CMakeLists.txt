find_package(pkg-config)

if(PKG_CONFIG_FOUND)
  pkg_get_variable(FISH_COMPLETIONS_DIR fish completionsdir)
endif()

if(NOT FISH_COMPLETIONS_DIR)
  set(FISH_COMPLETIONS_DIR ${CMAKE_INSTALL_DATADIR}/fish/vendor_completions.d)
endif()

set(MULTIPASS_INSTALL_FISH_COMPLETIONS_DIR ${FISH_COMPLETIONS_DIR} CACHE PATH "The directory where the fish shell completions are installed.")

install(
  FILES multipass.fish
  DESTINATION ${MULTIPASS_INSTALL_FISH_COMPLETIONS_DIR}
)
