HEADERS       = window.h
SOURCES       = main.cpp \
                window.cpp
RESOURCES     = systray.qrc

QT += widgets

# install
target.path = $$[QT_INSTALL_EXAMPLES]/widgets/desktop/systray
INSTALLS += target
