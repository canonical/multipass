install(
  FILES multipass.gui.autostart.desktop
  DESTINATION ${CMAKE_INSTALL_DATADIR}/multipass
)

install(
  FILES multipass.gui.desktop
  DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(
  FILES multipass.gui.svg
  DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)
