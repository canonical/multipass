# Copyright (C) Canonical, Ltd.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_executable(multipass_gui
  argparser.cpp
  client_gui.cpp
  main.cpp
  gui_cmd.cpp
  resources/resources.qrc)

set_target_properties(multipass_gui
  PROPERTIES
  OUTPUT_NAME multipass.gui)

target_link_libraries(multipass_gui
  client_common
  client_platform
  fmt
  formatter
  rpc
  settings
  qhotkey
  Qt6::Core
  Qt6::Network
  Qt6::Widgets)

install(TARGETS multipass_gui
  DESTINATION bin
  COMPONENT multipass_gui)

if(APPLE)
  # TODO: Convert this all into a proper Mac Application Bundle

  set_target_properties( multipass_gui PROPERTIES
    MACOSX_BUNDLE TRUE
    INSTALL_RPATH "@executable_path/../../../../lib")

  # Copy the cocoa platform plugin
  file(COPY ${_qt5Gui_install_prefix}/plugins/platforms/libqcocoa.dylib
    DESTINATION ${CMAKE_BINARY_DIR}/bin/multipass.gui.app/Contents/plugins/platforms)

  install(FILES ${CMAKE_BINARY_DIR}/bin/multipass.gui.app/Contents/plugins/platforms/libqcocoa.dylib
    DESTINATION bin/multipass.gui.app/Contents/plugins/platforms
    COMPONENT multipass_gui)

  # Copy any framework depenencies into the install directory for packaging
  install(CODE "
    include(${CMAKE_SOURCE_DIR}/packaging/FrameworkUtilities.cmake)
    install_frameworks(\"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/multipass.gui.app/Contents/MacOS/multipass.gui\" \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/lib\")
    " COMPONENT multipass_gui)

  install(CODE "
    include(${CMAKE_SOURCE_DIR}/packaging/FrameworkUtilities.cmake)
    install_frameworks(\"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/multipass.gui.app/Contents/plugins/platforms/libqcocoa.dylib\" \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/lib\" LOADER_PATH \"${_qt5Gui_install_prefix}/plugins/platforms\")
    " COMPONENT multipass_gui)

endif(APPLE)

if(MSVC)
  set(RESOURCE_GENERATED_SOURCE_DIR ${MULTIPASS_GENERATED_SOURCE_DIR}/multipass/resources)
  file(MAKE_DIRECTORY ${RESOURCE_GENERATED_SOURCE_DIR})
  configure_file(
    resources/gui.rc.in
    ${RESOURCE_GENERATED_SOURCE_DIR}/gui.rc
    @ONLY)

  target_sources(multipass_gui PRIVATE
    ${RESOURCE_GENERATED_SOURCE_DIR}/gui.rc)
  set_target_properties(multipass_gui PROPERTIES WIN32_EXECUTABLE true)

  # Copy the windows platform plugin
  install(FILES ${_qt5Gui_install_prefix}/plugins/platforms/qwindows.dll
    DESTINATION bin/platforms
    COMPONENT multipass_gui)

  install(CODE "
    include(BundleUtilities)
    fixup_bundle(
      \"\${CMAKE_INSTALL_PREFIX}/bin/multipass.gui.exe\"
      \"\${CMAKE_INSTALL_PREFIX}/bin/platforms/qwindows.dll\"
      \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}\")
    " COMPONENT multipass_gui)
endif()
