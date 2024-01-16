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
#include "mock_file_ops.h"

#include <multipass/mount_handler.h>
#include <multipass/vm_mount.h>

#include <filesystem>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{

struct PublicMountHandler : public mp::MountHandler
{
    template <typename... Ts>
    PublicMountHandler(Ts&&... params) : mp::MountHandler(std::forward<Ts>(params)...)
    {
    }

    void activate_impl(mp::ServerVariant server, std::chrono::milliseconds timeout) override
    {
    }

    void deactivate_impl(bool force) override
    {
    }
};

TEST(MountHandler, providesMountSpec)
{
    namespace fs = std::filesystem;

    mp::VMMount mount{"asdf", {}, {}, multipass::VMMount::MountType::Native};

    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, status).WillOnce(Return(fs::file_status{fs::file_type::directory, fs::perms::unknown}));

    PublicMountHandler handler{nullptr, nullptr, mount, ""};
    EXPECT_EQ(handler.get_mount_spec(), mount);
}

} // namespace
