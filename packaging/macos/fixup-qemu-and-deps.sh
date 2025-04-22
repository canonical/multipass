#!/bin/bash

set -euo pipefail

CMAKE_BINARY_DIR=$1
EXECUTABLE_PATH="@executable_path/../lib"

otool_qemu_system=$(otool -L ${CMAKE_BINARY_DIR}/bin/qemu-system-*)
otool_qemu_img=$(otool -L ${CMAKE_BINARY_DIR}/bin/qemu-img)

if ! echo $otool_qemu_system | grep -q @rpath && ! echo $otool_qemu_system | grep -q ${EXECUTABLE_PATH}; then
    dylibbundler -of -x ${CMAKE_BINARY_DIR}/bin/qemu-system-* -b -d ${CMAKE_BINARY_DIR}/lib/ -p @rpath -i /usr/lib
    install_name_tool -add_rpath "@executable_path/../lib" ${CMAKE_BINARY_DIR}/bin/qemu-system-*
fi

if ! echo $otool_qemu_img | grep -q @rpath && ! echo $otool_qemu_img | grep -q ${EXECUTABLE_PATH}; then
    dylibbundler -of -x ${CMAKE_BINARY_DIR}/bin/qemu-img -b -d ${CMAKE_BINARY_DIR}/lib/ -p @rpath -i /usr/lib
    install_name_tool -add_rpath "@executable_path/../lib" "${CMAKE_BINARY_DIR}/bin/qemu-img"
fi
