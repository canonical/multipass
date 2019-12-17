find_package(Qt5 COMPONENTS Core Widgets REQUIRED)

qt5_wrap_cpp(MOC_HEADERS
  QHotkey/QHotkey/qhotkey.h
  QHotkey/QHotkey/qhotkey_p.h
)

find_package(X11 REQUIRED)
find_package(Qt5X11Extras REQUIRED)

include_directories(${X11_INCLUDE_DIR})

add_library(qhotkey STATIC
  QHotkey/QHotkey/qhotkey.cpp
  QHotkey/QHotkey/qhotkey_x11.cpp
  ${MOC_HEADERS}
)

target_link_libraries(qhotkey
  Qt5::Core
  Qt5::Widgets
  Qt5::X11Extras
  ${X11_LIBRARIES}
)

target_include_directories(qhotkey
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/QHotkey/QHotkey
)
