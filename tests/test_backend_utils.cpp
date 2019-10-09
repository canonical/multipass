/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include <src/platform/backends/shared/linux/backend_utils.h>

#include "mock_process_factory.h"

#include <multipass/format.h>
#include <multipass/memory_size.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
QByteArray fake_img_info(const mp::MemorySize& size)
{
    return QByteArray::fromStdString(
        fmt::format("some\nother\ninfo\nfirst\nvirtual size: {} ({} bytes)\nmore\ninfo\nafter\n", size.in_gigabytes(),
                    size.in_bytes()));
}
} // namespace

TEST(BackendUtils, image_resizing_checks_minimum_size_and_proceeds_when_respected)
{
    const auto img = "/fake/img/path";
    const auto size = mp::MemorySize{"3G"};
    auto mock_factory_scope = mpt::MockProcessFactory::Inject();

    mock_factory_scope->register_callback([&img, &size](mpt::MockProcess* process) {
        static auto call_count = 0;

        ASSERT_EQ(process->program().toStdString(), "qemu-img");

        const auto args = process->arguments();
        mp::ProcessState success{0, mp::nullopt};
        if (++call_count == 1)
        {
            ASSERT_EQ(args.size(), 2);
            EXPECT_EQ(args.constFirst(), "info");
            EXPECT_EQ(args.constLast().toStdString(), img);

            InSequence s;
            EXPECT_CALL(*process, execute).WillOnce(Return(success));
            EXPECT_CALL(*process, read_all_standard_output).WillOnce(Return(fake_img_info(mp::MemorySize{"1G"})));
        }
        else
        {
            ASSERT_EQ(call_count, 2); // this should only be called twice
            ASSERT_EQ(args.size(), 3);
            EXPECT_EQ(args.at(0).toStdString(), "resize");
            EXPECT_EQ(args.at(1), img);
            EXPECT_THAT(args.at(2),
                        ResultOf([](const auto& val) { return mp::MemorySize{val.toStdString()}; }, Eq(size)));

            EXPECT_CALL(*process, execute).WillOnce(Return(success));
        }
    });

    mp::backend::resize_instance_image(size, img);
}
