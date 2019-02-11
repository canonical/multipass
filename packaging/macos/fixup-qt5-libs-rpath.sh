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

QT_FRAMEWORKS="QtCore QtNetwork"
BINARIES="multipass multipassd"

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

# Determine all the rpaths
RPATH_CHANGES=()
QT_VERSION=$( basename $( readlink /usr/local/opt/qt ) )
for framework in ${QT_FRAMEWORKS}; do
    framework_path="${framework}.framework/Versions/5/${framework}"
    RPATH_CHANGES+=("-change")
    RPATH_CHANGES+=("/usr/local/Cellar/qt/${QT_VERSION}/lib/${framework_path}")
    RPATH_CHANGES+=("@rpath/${framework_path}")

    RPATH_CHANGES+=("-change")
    RPATH_CHANGES+=("/usr/local/opt/qt/lib/${framework_path}")
    RPATH_CHANGES+=("@rpath/${framework_path}")
done

# Fix the libaries to use rpath
for framework in ${QT_FRAMEWORKS}; do
    framework_dir="${framework}.framework/Versions/5"
    framework_path="${framework_dir}/${framework}"
    mkdir -p "${TEMP_DIR}/${framework_dir}"
    cp -fv "/usr/local/opt/qt/lib/${framework_path}" \
           "${TEMP_DIR}/${framework_dir}"
    chmod +w "${TEMP_DIR}/${framework_path}"
    install_name_tool "${RPATH_CHANGES[@]}" "${TEMP_DIR}/${framework_path}"
    chmod -w "${TEMP_DIR}/${framework_path}"
done

# Edit the binaries to point to these newly edited libs
for binary in ${BINARIES}; do
    install_name_tool -add_rpath "${TEMP_DIR}" "${BINARY_DIR}/${binary}"
    install_name_tool "${RPATH_CHANGES[@]}" "${BINARY_DIR}/${binary}"
done