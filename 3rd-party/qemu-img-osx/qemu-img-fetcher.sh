#!/bin/bash

########################################################################################################################
# qemu-img-fetcher.sh  - downloads a pre-built qemu-img binary, plus all its dependencies, from HomeBrew
#
# All library paths need rewriting to suit the relative paths we need for a distributable binary.
########################################################################################################################

set -e

hash install_name_tool 2>/dev/null || { echo >&2 "Required tool 'install_name_tool' is missing. Aborting."; exit 1; }

if [ $# -ne 2 ]; then
    echo >&2 "Usage: $0 DOWNLOAD_DIR BINARIES_OUT_DIR";
    exit 1;
fi

DOWNLOAD_DIR="$1"
if [ ! -d "$DOWNLOAD_DIR" ]; then
    mkdir "$DOWNLOAD_DIR" || { echo >&2 "Unable to create download directory '$DOWNLOAD_DIR'. Aborting."; exit 1; }
fi;
OUT_DIR="$2"
if [ ! -d "$OUT_DIR" ]; then
    mkdir "$OUT_DIR" || { echo >&2 "Unable to create destination directory '$OUT_DIR'. Aborting."; exit 1; }
fi;

DIST="sierra"

BINDIR="${OUT_DIR}/bin"
LIBDIR="${OUT_DIR}/lib"

function pushd {
    command pushd "$@" > /dev/null
}
function popd {
    command popd "$@" > /dev/null
}

function get_brew_and_extract {
    pushd "$DOWNLOAD_DIR"
    FILENAME="$1.$DIST.bottle.tar.gz"
    if [ ! -f "$FILENAME" ]; then
        wget -O "$FILENAME" \
            "https://bintray.com/homebrew/bottles/download_file?file_path=$FILENAME"
    fi

    tar -xzf $FILENAME
    popd
}


# prepare directory tree
mkdir -p "$BINDIR"
mkdir -p "$LIBDIR"
mkdir -p "$DOWNLOAD_DIR"

# Get qemu-img binary (with shared lib requirements)
get_brew_and_extract qemu-4.1.0

cp "$DOWNLOAD_DIR/qemu/4.1.0/bin/qemu-img" "$BINDIR/"



# qemu-img requires these support libs: glib, gettext, nettle, gnutls, libssh
# Learn this from "otool -L qemu-img"

# glib - requres pcre
get_brew_and_extract glib-2.62.0_1

cp "$DOWNLOAD_DIR/glib/2.62.0_1/lib/libgthread-2.0.0.dylib" "$LIBDIR/"
cp "$DOWNLOAD_DIR/glib/2.62.0_1/lib/libglib-2.0.0.dylib"    "$LIBDIR/"

# pcre - required by glib (no other deps)
get_brew_and_extract pcre-8.43

cp "$DOWNLOAD_DIR/pcre/8.43/lib/libpcre.1.dylib" "$LIBDIR/"

# gettext (no other deps)
get_brew_and_extract gettext-0.20.1

cp "$DOWNLOAD_DIR/gettext/0.20.1/lib/libintl.8.dylib" "$LIBDIR/"

# nettle (no other deps)
get_brew_and_extract nettle-3.4.1

cp "$DOWNLOAD_DIR/nettle/3.4.1/lib/libnettle.6.dylib"  "$LIBDIR/"
cp "$DOWNLOAD_DIR/nettle/3.4.1/lib/libhogweed.4.dylib" "$LIBDIR/"  # needed by gnutls

# gnutls - requires p11-kit, libunistring, libtasn1, nettle (above), gmp, libintl (above), libidn2
get_brew_and_extract gnutls-3.6.9

cp "$DOWNLOAD_DIR/gnutls/3.6.9/lib/libgnutls.30.dylib" "$LIBDIR/"

# p11-kit - requires libffi
get_brew_and_extract p11-kit-0.23.17

cp "$DOWNLOAD_DIR/p11-kit/0.23.17/lib/libp11-kit.0.dylib" "$LIBDIR/"

# libffi (no other deps)
get_brew_and_extract libffi-3.2.1

cp "$DOWNLOAD_DIR/libffi/3.2.1/lib/libffi.6.dylib" "$LIBDIR/"

# libunistring (no other deps)
get_brew_and_extract libunistring-0.9.10

cp "$DOWNLOAD_DIR/libunistring/0.9.10/lib/libunistring.2.dylib" "$LIBDIR/"

# libtasn1 (no other deps)
get_brew_and_extract libtasn1-4.14

cp "$DOWNLOAD_DIR/libtasn1/4.14/lib/libtasn1.6.dylib" "$LIBDIR/"

# gmp (no other deps)
get_brew_and_extract gmp-6.1.2_2

cp "$DOWNLOAD_DIR/gmp/6.1.2_2/lib/libgmp.10.dylib" "$LIBDIR/"

# libidn2 - requires libintl (above), libunistring (above)
get_brew_and_extract libidn2-2.2.0_1

cp "$DOWNLOAD_DIR/libidn2/2.2.0_1/lib/libidn2.0.dylib" "$LIBDIR/"

# add writable permission to allow library path rewriting
chmod +w "$LIBDIR"/*.dylib
chmod +w "$BINDIR/qemu-img"


# Update a minimal set of library paths for "qemu-img convert" to work
install_name_tool -add_rpath "@executable_path/../lib/" "$BINDIR/qemu-img"
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/glib/lib/libgthread-2.0.0.dylib" "@rpath/libgthread-2.0.0.dylib"   "$BINDIR/qemu-img"
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/gnutls/lib/libgnutls.30.dylib"   "@rpath/libgnutls.30.dylib"       "$BINDIR/qemu-img"
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/nettle/lib/libnettle.6.dylib"    "@rpath/libnettle.6.dylib"        "$BINDIR/qemu-img"
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/gettext/lib/libintl.8.dylib"     "@rpath/libintl.8.dylib"          "$BINDIR/qemu-img"
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/glib/lib/libglib-2.0.0.dylib"    "@rpath/libglib-2.0.0.dylib"      "$BINDIR/qemu-img"

# We build our own libssh
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/libssh/lib/libssh.4.dylib"       "@rpath/libssh.4.dylib"           "$BINDIR/qemu-img"

install_name_tool -add_rpath "@loader_path/" "$LIBDIR/libgthread-2.0.0.dylib"
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/gettext/lib/libintl.8.dylib"     "@rpath/libintl.8.dylib"          "$LIBDIR/libgthread-2.0.0.dylib"
install_name_tool -change "@@HOMEBREW_CELLAR@@/glib/2.62.0_1/lib/libglib-2.0.0.dylib" "@rpath/libglib-2.0.0.dylib"    "$LIBDIR/libgthread-2.0.0.dylib"
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/pcre/lib/libpcre.1.dylib"        "@rpath/libpcre.1.dylib"          "$LIBDIR/libgthread-2.0.0.dylib"

install_name_tool -add_rpath "@loader_path/" "$LIBDIR/libglib-2.0.0.dylib"
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/gettext/lib/libintl.8.dylib"     "@rpath/libintl.8.dylib"          "$LIBDIR/libglib-2.0.0.dylib"
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/glib/lib/libglib-2.0.0.dylib"    "@rpath/libglib-2.0.0.dylib"      "$LIBDIR/libglib-2.0.0.dylib"
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/pcre/lib/libpcre.1.dylib"        "@rpath/libpcre.1.dylib"          "$LIBDIR/libglib-2.0.0.dylib"

install_name_tool -add_rpath "@loader_path/" "$LIBDIR/libgnutls.30.dylib"
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/p11-kit/lib/libp11-kit.0.dylib"  "@rpath/libp11-kit.0.dylib"         "$LIBDIR/libgnutls.30.dylib"
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/libunistring/lib/libunistring.2.dylib" "@rpath/libunistring.2.dylib" "$LIBDIR/libgnutls.30.dylib"
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/libtasn1/lib/libtasn1.6.dylib"   "@rpath/libtasn1.6.dylib"           "$LIBDIR/libgnutls.30.dylib"
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/nettle/lib/libnettle.6.dylib"    "@rpath/libnettle.6.dylib"          "$LIBDIR/libgnutls.30.dylib"
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/nettle/lib/libhogweed.4.dylib"   "@rpath/libhogweed.4.dylib"         "$LIBDIR/libgnutls.30.dylib"
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/gmp/lib/libgmp.10.dylib"         "@rpath/libgmp.10.dylib"            "$LIBDIR/libgnutls.30.dylib"
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/gettext/lib/libintl.8.dylib"     "@rpath/libintl.8.dylib"            "$LIBDIR/libgnutls.30.dylib"
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/libidn2/lib/libidn2.0.dylib"     "@rpath/libidn2.0.dylib"            "$LIBDIR/libgnutls.30.dylib"

install_name_tool -add_rpath "@loader_path/" "$LIBDIR/libhogweed.4.dylib"
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/gmp/lib/libgmp.10.dylib"         "@rpath/libgmp.10.dylib"          "$LIBDIR/libhogweed.4.dylib"
install_name_tool -change "@@HOMEBREW_CELLAR@@/nettle/3.4.1/lib/libnettle.6.dylib"    "@rpath/libnettle.6.dylib"      "$LIBDIR/libhogweed.4.dylib"

install_name_tool -add_rpath "@loader_path/" "$LIBDIR/libp11-kit.0.dylib"
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/libffi/lib/libffi.6.dylib"       "@rpath/libffi.6.dylib"           "$LIBDIR/libp11-kit.0.dylib"

install_name_tool -add_rpath "@loader_path/" "$LIBDIR/libidn2.0.dylib"
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/libunistring/lib/libunistring.2.dylib" "@rpath/libunistring.2.dylib" "$LIBDIR/libidn2.0.dylib"
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/gettext/lib/libintl.8.dylib"     "@rpath/libintl.8.dylib"            "$LIBDIR/libidn2.0.dylib"

# Update shared library name to be relative too
pushd "$LIBDIR"
for f in *.dylib; do
    install_name_tool -id "@rpath/$f" "$f"
done
popd

# remove writable permissions
chmod -w "$LIBDIR"/*.dylib
chmod -w "$BINDIR/qemu-img"
