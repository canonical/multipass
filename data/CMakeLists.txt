if(MULTIPASS_ENABLE_FLUTTER_GUI)
  install(
    FILES multipass.gui.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
  )
endif()

install(
  FILES multipass.gui.svg
  DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)
