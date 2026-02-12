#!/bin/bash

# Script: fixup-ssl-libs-rpath.sh
# The Brew SSL libraries have a problem: they're trickier to relocate as
# they're not compiled with rpath enabled.
# Ref: https://github.com/Homebrew/homebrew-core/issues/3219#issuecomment-235763791
#
# To make them work in a relocatable package, need to edit the paths to these libs in all
# the binaries to use rpaths.

set -euo pipefail

SSL_PATH="$(brew --prefix openssl@3)"
SSL_LIBS="ssl crypto"
BINARIES="bin/multipass bin/multipassd bin/sshfs_server lib/libdart_ffi.dylib"

if [ $# -ne 1 ]; then
    echo "Argument required"
    exit 1
fi

CMAKE_BINARY_DIR=$1
LIB_DIR="${CMAKE_BINARY_DIR}/lib"

# Determine all the rpaths
RPATH_CHANGES=()
for lib in ${SSL_LIBS}; do
    lib_path="$(perl -MCwd=realpath -e "print realpath '${SSL_PATH}/lib/lib${lib}.dylib'")"
    lib_name="$( basename ${lib_path} )"
    RPATH_CHANGES+=("-change")
    RPATH_CHANGES+=("${lib_path}")
    RPATH_CHANGES+=("@rpath/${lib_name}")

    RPATH_CHANGES+=("-change")
    RPATH_CHANGES+=("${SSL_PATH}/lib/${lib_name}")
    RPATH_CHANGES+=("@rpath/${lib_name}")
done

# Install and modify all the libraries
for lib in ${SSL_LIBS}; do
    lib_path="$(perl -MCwd=realpath -e "print realpath '${SSL_PATH}/lib/lib${lib}.dylib'")"
    lib_name="$( basename ${lib_path} )"
    target_path="${LIB_DIR}/${lib_name}"
    install -m 644 "${lib_path}" "${target_path}"
    install_name_tool "${RPATH_CHANGES[@]}" "${target_path}"
    chmod 444 "${target_path}"
done

# Edit the binaries to point to these newly edited libs
for binary in ${BINARIES}; do
    install_name_tool "${RPATH_CHANGES[@]}" "${CMAKE_BINARY_DIR}/${binary}"
done
