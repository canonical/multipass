/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "common.h"

#include <multipass/ssh/ssh_coordinates.h>
#include <multipass/utils/grpc_utils.h>

#include <cstdint>
#include <limits>
#include <variant>

namespace mp = multipass;
using namespace testing;

namespace
{
constexpr auto username = "ubuntu";
constexpr auto private_key = "base64-encoded-private-key";
constexpr auto tcp_host = "192.0.2.42";
constexpr uint32_t port = 4242;

void set_common_fields(mp::SSHCoordinatesInfo& proto)
{
    proto.set_username(username);
    proto.set_priv_key_base64(private_key);
    proto.set_port(port);
    proto.set_tcp_host(tcp_host);
}

void expect_common_fields(const mp::SSHCoordinatesInfo& proto)
{
    EXPECT_THAT(proto.username(), Eq(username));
    EXPECT_THAT(proto.priv_key_base64(), Eq(private_key));
    EXPECT_THAT(proto.port(), Eq(port));
    EXPECT_THAT(proto.tcp_host(), Eq(tcp_host));
}

void expect_common_fields(const mp::SSHCoordinates& coordinates)
{
    EXPECT_THAT(coordinates.username, Eq(username));
    EXPECT_THAT(coordinates.private_key_as_base64, Eq(private_key));
    EXPECT_THAT(coordinates.port, Eq(port));
    EXPECT_THAT(coordinates.tcp_host, Eq(tcp_host));
}

TEST(GrpcUtils, protoToCoordinatesCopiesCommonFields)
{
    mp::SSHCoordinatesInfo proto;
    set_common_fields(proto);

    expect_common_fields(mp::utils::proto_to_coordinates(proto));
}

TEST(GrpcUtils, coordinatesToProtoCopiesCommonFields)
{
    const mp::SSHCoordinates coordinates{username, private_key, port, tcp_host, std::monostate{}};

    expect_common_fields(mp::utils::coordinates_to_proto(coordinates));
}

TEST(GrpcUtils, protoToCoordinatesCopiesHvsockField)
{
    constexpr auto vmid = "hvsock-vm-id";
    mp::SSHCoordinatesInfo proto;
    proto.set_hvsock_vmid(vmid);

    const auto coordinates = mp::utils::proto_to_coordinates(proto);

    ASSERT_TRUE(std::holds_alternative<mp::HVSOCKData>(coordinates.vsock_host));
    EXPECT_THAT(std::get<mp::HVSOCKData>(coordinates.vsock_host).vmid, Eq(vmid));
}

TEST(GrpcUtils, coordinatesToProtoCopiesHvsockField)
{
    constexpr auto vmid = "hvsock-vm-id";
    const mp::SSHCoordinates coordinates{username,
                                         private_key,
                                         port,
                                         tcp_host,
                                         mp::HVSOCKData{vmid}};

    const auto proto = mp::utils::coordinates_to_proto(coordinates);

    EXPECT_THAT(proto.vsock_host_case(), Eq(mp::SSHCoordinatesInfo::kHvsockVmid));
    EXPECT_THAT(proto.hvsock_vmid(), Eq(vmid));
}

TEST(GrpcUtils, protoToCoordinatesCopiesVsockField)
{
    constexpr uint32_t cid = 424242;
    mp::SSHCoordinatesInfo proto;
    proto.set_vsock_cid(cid);

    const auto coordinates = mp::utils::proto_to_coordinates(proto);

    ASSERT_TRUE(std::holds_alternative<mp::VSOCKData>(coordinates.vsock_host));
    EXPECT_THAT(std::get<mp::VSOCKData>(coordinates.vsock_host).cid, Eq(cid));
}

TEST(GrpcUtils, coordinatesToProtoCopiesVsockField)
{
    constexpr uint32_t cid = 424242;
    const mp::SSHCoordinates coordinates{username, private_key, port, tcp_host, mp::VSOCKData{cid}};

    const auto proto = mp::utils::coordinates_to_proto(coordinates);

    EXPECT_THAT(proto.vsock_host_case(), Eq(mp::SSHCoordinatesInfo::kVsockCid));
    EXPECT_THAT(proto.vsock_cid(), Eq(cid));
}

TEST(GrpcUtils, protoToCoordinatesCopiesUsockField)
{
    constexpr auto socket_address = "/run/multipass/test.socket";
    mp::SSHCoordinatesInfo proto;
    proto.set_usock_addr(socket_address);

    const auto coordinates = mp::utils::proto_to_coordinates(proto);

    ASSERT_TRUE(std::holds_alternative<mp::USOCKData>(coordinates.vsock_host));
    EXPECT_THAT(std::get<mp::USOCKData>(coordinates.vsock_host).socket_address, Eq(socket_address));
}

TEST(GrpcUtils, coordinatesToProtoCopiesUsockField)
{
    constexpr auto socket_address = "/run/multipass/test.socket";
    const mp::SSHCoordinates coordinates{username,
                                         private_key,
                                         port,
                                         tcp_host,
                                         mp::USOCKData{socket_address}};

    const auto proto = mp::utils::coordinates_to_proto(coordinates);

    EXPECT_THAT(proto.vsock_host_case(), Eq(mp::SSHCoordinatesInfo::kUsockAddr));
    EXPECT_THAT(proto.usock_addr(), Eq(socket_address));
}

TEST(GrpcUtils, protoToCoordinatesCopiesNoHostField)
{
    mp::SSHCoordinatesInfo proto;

    const auto coordinates = mp::utils::proto_to_coordinates(proto);

    EXPECT_TRUE(std::holds_alternative<std::monostate>(coordinates.vsock_host));
}

TEST(GrpcUtils, coordinatesToProtoClearsNoHostField)
{
    const mp::SSHCoordinates coordinates{username, private_key, port, tcp_host, std::monostate{}};

    const auto proto = mp::utils::coordinates_to_proto(coordinates);

    EXPECT_THAT(proto.vsock_host_case(), Eq(mp::SSHCoordinatesInfo::VSOCK_HOST_NOT_SET));
}

TEST(GrpcUtils, portBoundaryValuesSurviveRoundTripWithoutVsockHost)
{
    for (const uint32_t boundary_port : {uint32_t{0}, std::numeric_limits<uint32_t>::max()})
    {
        const mp::SSHCoordinates coordinates{username,
                                             private_key,
                                             boundary_port,
                                             tcp_host,
                                             std::monostate{}};

        const auto round_trip = mp::utils::proto_to_coordinates(
            mp::utils::coordinates_to_proto(coordinates));

        EXPECT_THAT(round_trip.port, Eq(boundary_port));
        EXPECT_TRUE(std::holds_alternative<std::monostate>(round_trip.vsock_host));
    }
}

TEST(GrpcUtils, vsockCidBoundaryValuesSurviveRoundTrip)
{
    for (const uint32_t cid : {uint32_t{0}, std::numeric_limits<uint32_t>::max()})
    {
        const mp::SSHCoordinates coordinates{username,
                                             private_key,
                                             port,
                                             tcp_host,
                                             mp::VSOCKData{cid}};

        const auto round_trip = mp::utils::proto_to_coordinates(
            mp::utils::coordinates_to_proto(coordinates));

        ASSERT_TRUE(std::holds_alternative<mp::VSOCKData>(round_trip.vsock_host));
        EXPECT_THAT(std::get<mp::VSOCKData>(round_trip.vsock_host).cid, Eq(cid));
    }
}
} // namespace
