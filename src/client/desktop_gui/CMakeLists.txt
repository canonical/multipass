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

if (${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "x86_64")
  set(DESKTOP_GUI_ARCH x64)
else()
  set(DESKTOP_GUI_ARCH arm64)
endif()

if(UNIX AND NOT APPLE) # Linux
  set(DESKTOP_GUI_TARGET linux)
  set(DESKTOP_GUI_PREFIX ${DESKTOP_GUI_TARGET}/${DESKTOP_GUI_ARCH}/release)
  set(DESKTOP_GUI_BUNDLE_RELATIVE bundle)
  set(DESKTOP_GUI_EXEC_PATH_RELATIVE_TO_BUNDLE "")
  set(PROTOC_DART_PLUGIN_PATH $ENV{HOME}/.pub-cache/bin/protoc-gen-dart)
endif()

set(DESKTOP_GUI_BUILD_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
cmake_path(RELATIVE_PATH DESKTOP_GUI_BUILD_DIR)
set(DESKTOP_GUI_PATH ${DESKTOP_GUI_BUILD_DIR}/${DESKTOP_GUI_PREFIX})
set(DESKTOP_GUI_BUNDLE ${DESKTOP_GUI_PATH}/${DESKTOP_GUI_BUNDLE_RELATIVE})
set(DESKTOP_GUI_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/client/desktop_gui)
set(DESKTOP_GUI_EXECUTABLE desktop_gui)
set(DESKTOP_GUI_LINK_NAME multipass.desktop-gui)
set(DESKTOP_GUI_EXECUTABLE_FULL_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${DESKTOP_GUI_PREFIX}/${DESKTOP_GUI_BUNDLE_RELATIVE}/${DESKTOP_GUI_EXEC_PATH_RELATIVE_TO_BUNDLE}/${DESKTOP_GUI_EXECUTABLE})

file(GLOB DESKTOP_GUI_SOURCE_FILES ${DESKTOP_GUI_SOURCE_DIR}/lib/*.dart)

set(FLUTTER_BUILD_ARGS --release --suppress-analytics --verbose)

add_custom_command(OUTPUT ${DESKTOP_GUI_EXECUTABLE_FULL_PATH}
  DEPENDS ${DESKTOP_GUI_SOURCE_FILES}
  WORKING_DIRECTORY ${DESKTOP_GUI_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND} -E make_directory lib/generated
  COMMAND ${DART_EXECUTABLE} pub global activate protoc_plugin
  COMMAND ${Protobuf_PROTOC_EXECUTABLE} --dart_out=grpc:lib/generated -I ../../rpc ../../rpc/multipass.proto --plugin=${PROTOC_DART_PLUGIN_PATH}
  COMMAND ${FLUTTER_EXECUTABLE} config --build-dir=${DESKTOP_GUI_BUILD_DIR}
  COMMAND ${FLUTTER_EXECUTABLE} ARGS build ${DESKTOP_GUI_TARGET} ${FLUTTER_BUILD_ARGS}
  COMMAND ${FLUTTER_EXECUTABLE} config --build-dir=
)

add_custom_target(${DESKTOP_GUI_EXECUTABLE} ALL
  DEPENDS ${DESKTOP_GUI_EXECUTABLE_FULL_PATH}
)

if(UNIX AND NOT APPLE) # Linux
  # This creates the link to the executable inside the bundle directory, but will work only after install.
  add_custom_target(${DESKTOP_GUI_LINK_NAME} ALL
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${DESKTOP_GUI_EXECUTABLE_FULL_PATH} ${DESKTOP_GUI_LINK_NAME})

  # This is just to make it work without installing.
  add_custom_target(bundle_symlink ALL
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${DESKTOP_GUI_PREFIX}/${DESKTOP_GUI_BUNDLE_RELATIVE} ${DESKTOP_GUI_BUNDLE_RELATIVE}
  )

  # Install the symlink.
  install(FILES ${DESKTOP_GUI_BUILD_DIR}/${DESKTOP_GUI_LINK_NAME}
    DESTINATION bin
  )
endif()

install(DIRECTORY ${DESKTOP_GUI_BUNDLE}
  DESTINATION bin
  USE_SOURCE_PERMISSIONS
)
