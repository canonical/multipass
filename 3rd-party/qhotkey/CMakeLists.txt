find_package(Qt6 COMPONENTS Core Widgets REQUIRED)

qt6_wrap_cpp(MOC_HEADERS
  QHotkey/QHotkey/qhotkey.h
  QHotkey/QHotkey/qhotkey_p.h
)

add_library(qhotkey STATIC
  QHotkey/QHotkey/qhotkey.cpp
  ${MOC_HEADERS}
)

target_link_libraries(qhotkey
  Qt6::Core
  Qt6::Widgets
)

if(APPLE)
  find_library(CARBON_LIBRARY Carbon)
  mark_as_advanced(CARBON_LIBRARY)

  target_sources(qhotkey
    PRIVATE
      QHotkey/QHotkey/qhotkey_mac.cpp
  )

  target_link_libraries(qhotkey
    ${CARBON_LIBRARY}
  )

elseif(WIN32)
  target_sources(qhotkey
    PRIVATE
      QHotkey/QHotkey/qhotkey_win.cpp
  )

else()
  find_package(X11 REQUIRED)

  include_directories(${X11_INCLUDE_DIR})

  target_sources(qhotkey
    PRIVATE
      QHotkey/QHotkey/qhotkey_x11.cpp
  )

  target_link_libraries(qhotkey
    ${X11_LIBRARIES}
  )

endif()

target_include_directories(qhotkey
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/QHotkey/QHotkey
)
