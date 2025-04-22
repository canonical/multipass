#!/bin/bash

# Script: fixup-qt6-libs-rpath.sh
# If using the Brew Qt6 libraries, they have a problem: they're trickier to relocate as
# they're not compiled with rpath enabled.
# Ref: https://github.com/Homebrew/homebrew-core/issues/3219#issuecomment-235763791
#
# To make them work in a relocatable package, need to edit the paths to these libs in all
# the binaries to use rpaths.
#
# The libs shipped by Qt do not have this issue.

set -u

QT_FRAMEWORKS="QtCore QtNetwork QtWidgets QtDBus QtPrintSupport"
BINARIES="multipass multipassd sshfs_server"
QT6_PATH="$(brew --prefix qt6)"

if [ $# -ne 1 ]; then
    echo "Argument required"
    exit 1;
fi

CMAKE_BINARY_DIR=$1
BINARY_DIR="$1/bin"

# Check if brew libs used. If not, nothing to do
if ! otool -L "$BINARY_DIR/multipass" | grep -q "${QT6_PATH}"; then
    echo "Not using Qt6 from Homebrew, nothing to do"
    exit 0;
fi

# Copy libs to temporary location
LIB_DIR="$CMAKE_BINARY_DIR/lib"

RPATH_CHANGES=()
QT_VERSION=$( basename $( readlink ${QT6_PATH} ) )
CELLAR_PATH="$(brew --prefix)/Cellar/qt"
for framework in ${QT_FRAMEWORKS}; do
    framework_dir="${framework}.framework/Versions/A"
    framework_path="${framework_dir}/${framework}"

    RPATH_CHANGES+=("-change")
    RPATH_CHANGES+=("${QT6_PATH}/${QT_VERSION}/lib/${framework_path}")
    RPATH_CHANGES+=("@rpath/${framework_path}")

    RPATH_CHANGES+=("-change")
    RPATH_CHANGES+=("${QT6_PATH}/lib/${framework_path}")
    RPATH_CHANGES+=("@rpath/${framework_path}")

    RPATH_CHANGES+=("-change")
    RPATH_CHANGES+=("${CELLAR_PATH}/${QT_VERSION}/lib/${framework_path}")
    RPATH_CHANGES+=("@rpath/${framework_path}")

    mkdir -p "${LIB_DIR}/${framework_dir}"
    cp -fv "${QT6_PATH}/lib/${framework_path}" \
           "${LIB_DIR}/${framework_dir}"
    chmod +w "${LIB_DIR}/${framework_path}"

    dylibbundler -of -x "${LIB_DIR}/${framework_path}" -b -d ${CMAKE_BINARY_DIR}/lib/ -p @rpath -i /usr/lib/ -i @rpath -i @executable_path -i @loader_path

    # Add rpaths for package
    install_name_tool -add_rpath "@loader_path/../../../" "${LIB_DIR}/${framework_path}"

    install_name_tool "-id" "@rpath/${framework_path}" "${LIB_DIR}/${framework_path}"

    QT6_DEPS=$(otool -L ${QT6_PATH}/lib/${framework_path} | grep ${CELLAR_PATH} | awk -F '\t' -F ' ' '{ print $1 }')
    for dep in ${QT6_DEPS:-}; do
        BASE_DEP=$( basename ${dep} )
        install_name_tool "-change" "${dep}" "@rpath/${BASE_DEP}.framework/Versions/A/${BASE_DEP}" "${LIB_DIR}/${framework_path}"
    done

    chmod -w "${LIB_DIR}/${framework_path}"
done

# Edit the binaries to point to these newly edited libs
for binary in ${BINARIES}; do
    install_name_tool "${RPATH_CHANGES[@]}" "${BINARY_DIR}/${binary}"
done

