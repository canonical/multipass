#!/bin/bash

set -euo pipefail

CMAKE_BINARY_DIR=$1
EXECUTABLE_PATH="@executable_path/../lib"

if ! otool -L ${CMAKE_BINARY_DIR}/bin/qemu-system-* | grep -q ${EXECUTABLE_PATH}; then
    dylibbundler -of -x ${CMAKE_BINARY_DIR}/bin/qemu-system-* -b -d ${CMAKE_BINARY_DIR}/lib/ -p @executable_path/../lib -i /usr/lib
fi

if ! otool -L "${CMAKE_BINARY_DIR}/bin/qemu-img" | grep -q "${EXECUTABLE_PATH}"; then
    dylibbundler -of -x ${CMAKE_BINARY_DIR}/bin/qemu-img -b -d ${CMAKE_BINARY_DIR}/lib/ -p @executable_path/../lib -i /usr/lib
fi

# Resign the binary since we modify it above
codesign --entitlements ${CMAKE_BINARY_DIR}/3rd-party/qemu/accel/hvf/entitlements.plist --force -s - ${CMAKE_BINARY_DIR}/bin/qemu-system-*
