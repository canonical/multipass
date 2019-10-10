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

void simulate_qemuimg_info(const mpt::MockProcess* process, const QString& expect_img,
                           const mp::ProcessState& produce_result, const QByteArray& produce_output = {})
{
    ASSERT_EQ(process->program().toStdString(), "qemu-img");

    const auto args = process->arguments();
    ASSERT_EQ(args.size(), 2);

    EXPECT_EQ(args.constFirst(), "info");
    EXPECT_EQ(args.constLast(), expect_img);

    InSequence s;

    EXPECT_CALL(*process, execute).WillOnce(Return(produce_result));
    if (produce_result.completed_successfully())
        EXPECT_CALL(*process, read_all_standard_output).WillOnce(Return(produce_output));
    else if (produce_result.exit_code)
        EXPECT_CALL(*process, read_all_standard_error).WillOnce(Return(produce_output));
    else
        ON_CALL(*process, read_all_standard_error).WillByDefault(Return(produce_output));
}

void simulate_qemuimg_resize(const mpt::MockProcess* process, const QString& expect_img,
                             const mp::MemorySize& expect_size, const mp::ProcessState& produce_result)
{
    ASSERT_EQ(process->program().toStdString(), "qemu-img");

    const auto args = process->arguments();
    ASSERT_EQ(args.size(), 3);

    EXPECT_EQ(args.at(0).toStdString(), "resize");
    EXPECT_EQ(args.at(1), expect_img);
    EXPECT_THAT(args.at(2),
                ResultOf([](const auto& val) { return mp::MemorySize{val.toStdString()}; }, Eq(expect_size)));

    EXPECT_CALL(*process, execute).WillOnce(Return(produce_result));
}

} // namespace

TEST(BackendUtils, image_resizing_checks_minimum_size_and_proceeds_when_respected)
{
    const auto img = "/fake/img/path";
    const auto requested_size = mp::MemorySize{"3G"};
    auto process_count = 0;
    auto mock_factory_scope = mpt::MockProcessFactory::Inject();

    mock_factory_scope->register_callback([&img, &requested_size, &process_count](mpt::MockProcess* process) {
        mp::ProcessState success{0, mp::nullopt};

        ASSERT_LE(++process_count, 2);
        if (process_count == 1)
            simulate_qemuimg_info(process, img, success, fake_img_info(mp::MemorySize{"1G"}));
        else
            simulate_qemuimg_resize(process, img, requested_size, success);
    });

    mp::backend::resize_instance_image(requested_size, img);
    ASSERT_EQ(process_count, 2);
}

TEST(BackendUtils, image_resizing_not_attempted_when_qemuimg_crashes_on_info)
{
    const auto img = "foobar";
    auto mock_factory_scope = mpt::MockProcessFactory::Inject();

    const auto qemu_msg = "about to crash";
    const auto system_msg = "core dumped";

    mock_factory_scope->register_callback(
        [&img, &qemu_msg, &system_msg, process_count = 0](mpt::MockProcess* process) mutable {
            ASSERT_EQ(++process_count, 1);

            mp::ProcessState crash{mp::nullopt, mp::ProcessState::Error{QProcess::Crashed, system_msg}};
            simulate_qemuimg_info(process, img, crash, qemu_msg);
        });

    try
    {
        mp::backend::resize_instance_image(mp::MemorySize{}, img);
        ADD_FAILURE();
    }
    catch (std::runtime_error& e)
    {
        const auto msg = e.what();
        EXPECT_THAT(msg, HasSubstr("qemu-img failed"));
        EXPECT_THAT(msg, HasSubstr(qemu_msg));
        EXPECT_THAT(msg, HasSubstr(system_msg));
    }
    catch (...)
    {
        ADD_FAILURE();
    }
}
