# Copyright © 2017-2026 Canonical Ltd.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.14)
project(xz-embedded LANGUAGES C)
include(GNUInstallDirs)

add_library(xz-embedded STATIC
    linux/lib/xz/xz_crc32.c
    linux/lib/xz/xz_crc64.c
    linux/lib/xz/xz_dec_lzma2.c
    linux/lib/xz/xz_dec_stream.c
)
add_library(xz-embedded::xz-embedded ALIAS xz-embedded)

target_compile_definitions(xz-embedded PUBLIC XZ_USE_CRC64)
target_include_directories(xz-embedded
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/userspace"
        "${CMAKE_CURRENT_SOURCE_DIR}/linux/include/linux"
        "${CMAKE_CURRENT_SOURCE_DIR}/linux/lib/xz"
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/linux/include/linux>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

install(TARGETS xz-embedded EXPORT xz-embedded-targets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(FILES linux/include/linux/xz.h
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(EXPORT xz-embedded-targets
        FILE xz-embedded-config.cmake
        NAMESPACE xz-embedded::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/xz-embedded")