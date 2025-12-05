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

get_filename_component(MULTIPASS_PROTOCOL_SPEC_ABS_PATH multipass.proto ABSOLUTE)
set(GRPC_GENERATED_SOURCE_DIR ${MULTIPASS_GENERATED_SOURCE_DIR}/multipass/rpc)
file(MAKE_DIRECTORY ${GRPC_GENERATED_SOURCE_DIR})

if(NOT MSVC)
  add_compile_options(-Wno-error=pedantic)
else()
  add_compile_options(/bigobj)
endif()

add_library(rpc STATIC)

# 1) Generate the .pb.cc/.pb.h
protobuf_generate(
    TARGET rpc
    LANGUAGE cpp
    PROTOS ${MULTIPASS_PROTOCOL_SPEC_ABS_PATH}
    PROTOC_OUT_DIR ${GRPC_GENERATED_SOURCE_DIR}
)

 # 2) Generate the .grpc.pb.cc/.grpc.pb.h
protobuf_generate(
    TARGET rpc
    LANGUAGE grpc
    PROTOS ${MULTIPASS_PROTOCOL_SPEC_ABS_PATH}
    PROTOC_OUT_DIR ${GRPC_GENERATED_SOURCE_DIR}
    PLUGIN protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
)
# protobuf_generate adds the generated stuff via target_sources.
# Mark the generated folder  as SYSTEM so the linters and other
# tooling won't complain.
target_include_directories(rpc SYSTEM PUBLIC ${GRPC_GENERATED_SOURCE_DIR})
target_link_libraries(rpc gRPC)
