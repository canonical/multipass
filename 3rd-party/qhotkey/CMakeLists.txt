find_package(Qt6 COMPONENTS Core Widgets REQUIRED)

qt6_wrap_cpp(MOC_HEADERS
  QHotkey/QHotkey/qhotkey.h
  QHotkey/QHotkey/qhotkey_p.h
)

find_package(X11 REQUIRED)

include_directories(${X11_INCLUDE_DIR})

add_library(qhotkey STATIC
  QHotkey/QHotkey/qhotkey.cpp
  QHotkey/QHotkey/qhotkey_x11.cpp
  ${MOC_HEADERS}
)

target_link_libraries(qhotkey
  Qt6::Core
  Qt6::Widgets
  ${X11_LIBRARIES}
)

target_include_directories(qhotkey
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/QHotkey/QHotkey
)
