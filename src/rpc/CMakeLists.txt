# Copyright (C) Canonical, Ltd.
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

set(MULTIPASS_PROTOCOL_SPEC multipass.proto)
set(GRPC_GENERATED_SOURCE_DIR ${MULTIPASS_GENERATED_SOURCE_DIR}/multipass/rpc)
file(MAKE_DIRECTORY ${GRPC_GENERATED_SOURCE_DIR})

add_compile_options(-Wno-error=pedantic)

generate_grpc_cpp(GRPC_GENERATED_SOURCES ${GRPC_GENERATED_SOURCE_DIR} ${MULTIPASS_PROTOCOL_SPEC})

add_library(rpc STATIC
  ${GRPC_GENERATED_SOURCES})

target_link_libraries(rpc
  gRPC)
