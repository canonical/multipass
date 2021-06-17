#!/bin/bash

set -euo pipefail

CMAKE_BINARY_DIR=$1

dylibbundler -of -x ${CMAKE_BINARY_DIR}/bin/qemu-system-* -b -d ${CMAKE_BINARY_DIR}/lib/ -p @executable_path/../lib -i /usr/lib
dylibbundler -of -x ${CMAKE_BINARY_DIR}/bin/qemu-img -b -d ${CMAKE_BINARY_DIR}/lib/ -p @executable_path/../lib -i /usr/lib
