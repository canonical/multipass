#!/bin/bash

# Script: fixup-qt5-libs-rpath.sh
# If using the Brew Qt5 libraries, they have a problem: they're trickier to relocate as
# they're not compiled with rpath enabled.
# Ref: https://github.com/Homebrew/homebrew-core/issues/3219#issuecomment-235763791
#
# To make them work in a relocatable package, need to edit the paths to these libs in all
# the binaries to use rpaths.
#
# The libs shipped by Qt do not have this issue.

set -u

if [ $# -ne 1 ]; then
    echo "Argument required"
    exit 1;
fi

CMAKE_BINARY_DIR=$1
BINARY_DIR="$1/bin"

# Check if brew libs used. If not, nothing to do
if ! otool -L "$BINARY_DIR/multipass" | grep -q "/usr/local/opt/qt"; then
    echo "Not using Qt5 from Homebrew, nothing to do"
    exit 0;
fi

# Copy libs to temporary location
TEMP_DIR="$CMAKE_BINARY_DIR/fixup-qt5-libs"
mkdir -p "$TEMP_DIR"

# QtCore
mkdir -p "${TEMP_DIR}/QtCore.framework/Versions/5"
cp -fv "/usr/local/opt/qt/lib/QtCore.framework/Versions/5/QtCore" "${TEMP_DIR}/QtCore.framework/Versions/5/"

# QtNetwork
mkdir -p "${TEMP_DIR}/QtNetwork.framework/Versions/5"
cp -fv "/usr/local/opt/qt/lib/QtNetwork.framework/Versions/5/QtNetwork" "${TEMP_DIR}/QtNetwork.framework/Versions/5/"
chmod +w "${TEMP_DIR}/QtNetwork.framework/Versions/5/QtNetwork"
install_name_tool -change "/usr/local/Cellar/qt/5.11.1/lib/QtCore.framework/Versions/5/QtCore" \
    "@rpath/QtCore.framework/Versions/5/QtCore" \
    "${TEMP_DIR}/QtNetwork.framework/Versions/5/QtNetwork"
chmod -w "${TEMP_DIR}/QtNetwork.framework/Versions/5/QtNetwork"

# Edit the binaries to point to these newly edited libs
install_name_tool -add_rpath "$TEMP_DIR" "$BINARY_DIR/multipass"
install_name_tool -change "/usr/local/opt/qt/lib/QtCore.framework/Versions/5/QtCore" "@rpath/QtCore.framework/Versions/5/QtCore" "$BINARY_DIR/multipass"
install_name_tool -change "/usr/local/opt/qt/lib/QtNetwork.framework/Versions/5/QtNetwork" "@rpath/QtNetwork.framework/Versions/5/QtNetwork" "$BINARY_DIR/multipass"

install_name_tool -add_rpath "$TEMP_DIR" "$BINARY_DIR/multipassd"
install_name_tool -change "/usr/local/opt/qt/lib/QtCore.framework/Versions/5/QtCore" "@rpath/QtCore.framework/Versions/5/QtCore" "$BINARY_DIR/multipassd"
install_name_tool -change "/usr/local/opt/qt/lib/QtNetwork.framework/Versions/5/QtNetwork" "@rpath/QtNetwork.framework/Versions/5/QtNetwork" "$BINARY_DIR/multipassd"
