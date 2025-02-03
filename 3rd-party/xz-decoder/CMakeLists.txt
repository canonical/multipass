# Copyright © 2018-2022 Canonical Ltd.
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

include_directories(
  xz-embedded/linux/include/linux
  xz-embedded/userspace)

add_definitions(
  -DXZ_USE_CRC64)

add_library(xz-embedded STATIC
  xz-embedded/linux/lib/xz/xz_crc32.c
  xz-embedded/linux/lib/xz/xz_crc64.c
  xz-embedded/linux/lib/xz/xz_dec_lzma2.c
  xz-embedded/linux/lib/xz/xz_dec_stream.c
)

set_property(TARGET xz-embedded PROPERTY POSITION_INDEPENDENT_CODE ON)

target_include_directories(xz-embedded INTERFACE
  xz-embedded/linux/include/linux)
