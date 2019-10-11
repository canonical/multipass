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

#include "extra_assertions.h"
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
const auto success = mp::ProcessState{0, mp::nullopt};
const auto null_string_matcher = static_cast<mp::optional<decltype(_)>>(mp::nullopt);

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

template <class Matcher>
void test_image_resizing(const char* img, const mp::MemorySize& img_virtual_size, const mp::MemorySize& requested_size,
                         const mp::ProcessState& qemuimg_info_result, bool attempt_resize,
                         const mp::ProcessState& qemuimg_resize_result, mp::optional<Matcher> throw_msg_matcher)
{
    auto process_count = 0;
    auto mock_factory_scope = mpt::MockProcessFactory::Inject();
    const auto expected_final_process_count = attempt_resize ? 2 : 1;

    mock_factory_scope->register_callback([&](mpt::MockProcess* process) {
        ASSERT_LE(++process_count, expected_final_process_count);
        if (process_count == 1)
            simulate_qemuimg_info(process, img, qemuimg_info_result, fake_img_info(img_virtual_size));
        else
            simulate_qemuimg_resize(process, img, requested_size, qemuimg_resize_result);
    });

    if (throw_msg_matcher)
        MP_EXPECT_THROW_THAT(mp::backend::resize_instance_image(requested_size, img), std::runtime_error,
                             Property(&std::runtime_error::what, *throw_msg_matcher));
    else
        mp::backend::resize_instance_image(requested_size, img);

    EXPECT_EQ(process_count, expected_final_process_count);
}
} // namespace

TEST(BackendUtils, image_resizing_checks_minimum_size_and_proceeds_when_larger)
{
    const auto img = "/fake/img/path";
    const auto min_size = mp::MemorySize{"1G"};
    const auto request_size = mp::MemorySize{"3G"};
    const auto qemuimg_info_result = success;
    const auto attempt_resize = true;
    const auto qemuimg_resize_result = success;
    const auto throw_msg_matcher = null_string_matcher;

    test_image_resizing(img, min_size, request_size, qemuimg_info_result, attempt_resize, qemuimg_resize_result,
                        throw_msg_matcher);
}

TEST(BackendUtils, image_resizing_checks_minimum_size_and_proceeds_when_equal)
{
    const auto img = "/fake/img/path";
    const auto min_size = mp::MemorySize{"1234554321"};
    const auto request_size = min_size;
    const auto qemuimg_info_result = success;
    const auto attempt_resize = true;
    const auto qemuimg_resize_result = success;
    const auto throw_msg_matcher = null_string_matcher;

    test_image_resizing(img, min_size, request_size, qemuimg_info_result, attempt_resize, qemuimg_resize_result,
                        throw_msg_matcher);
}

TEST(BackendUtils, image_resizing_not_attempted_when_below_minimum)
{
    const auto img = "SomeImg";
    const auto min_size = mp::MemorySize{"2200M"};
    const auto request_size = mp::MemorySize{"2G"};
    const auto qemuimg_info_result = success;
    const auto attempt_resize = false;
    const auto qemuimg_resize_result = success;
    const auto throw_msg_matcher = mp::make_optional(HasSubstr("below minimum"));

    test_image_resizing(img, min_size, request_size, qemuimg_info_result, attempt_resize, qemuimg_resize_result,
                        throw_msg_matcher);
}

TEST(BackendUtils, image_resize_detects_resizing_failure_and_throws)
{
    const auto img = "imagine";
    const auto min_size = mp::MemorySize{"100M"};
    const auto request_size = mp::MemorySize{"400M"};
    const auto qemuimg_info_result = success;
    const auto attempt_resize = true;
    const auto qemuimg_resize_result = mp::ProcessState{42, mp::nullopt};
    const auto throw_msg_matcher = mp::make_optional(HasSubstr("qemu-img failed"));

    test_image_resizing(img, min_size, request_size, qemuimg_info_result, attempt_resize, qemuimg_resize_result,
                        throw_msg_matcher);
}

TEST(BackendUtils, image_resizing_not_attempted_when_qemuimg_info_crashes)
{
    const auto img = "foo";
    const auto qemu_msg = "about to crash";
    const auto system_msg = "core dumped";

    auto mock_factory_scope = mpt::MockProcessFactory::Inject();
    mock_factory_scope->register_callback(
        [&img, &qemu_msg, &system_msg, process_count = 0](mpt::MockProcess* process) mutable {
            ASSERT_EQ(++process_count, 1);

            mp::ProcessState crash{mp::nullopt, mp::ProcessState::Error{QProcess::Crashed, system_msg}};
            simulate_qemuimg_info(process, img, crash, qemu_msg);
        });

    MP_EXPECT_THROW_THAT(mp::backend::resize_instance_image(mp::MemorySize{}, img), std::runtime_error,
                         Property(&std::runtime_error::what,
                                  AllOf(HasSubstr("qemu-img failed"), HasSubstr(qemu_msg), HasSubstr(system_msg))));
}

TEST(BackendUtils, image_resizing_not_attempted_when_img_not_found)
{
    const auto img = "bar";
    const auto qemu_msg = "not found";

    auto mock_factory_scope = mpt::MockProcessFactory::Inject();
    mock_factory_scope->register_callback([&img, &qemu_msg, process_count = 0](mpt::MockProcess* process) mutable {
        ASSERT_EQ(++process_count, 1);

        mp::ProcessState failure{1, mp::nullopt};
        simulate_qemuimg_info(process, img, failure, qemu_msg);
    });

    MP_EXPECT_THROW_THAT(mp::backend::resize_instance_image(mp::MemorySize{"12345M"}, img), std::runtime_error,
                         Property(&std::runtime_error::what, HasSubstr(qemu_msg)));
}

TEST(BackendUtils, image_resizing_not_attempted_when_minimum_size_not_understood)
{
    const auto img = "baz";

    auto mock_factory_scope = mpt::MockProcessFactory::Inject();
    mock_factory_scope->register_callback([&img, process_count = 0](mpt::MockProcess* process) mutable {
        ASSERT_EQ(++process_count, 1);
        simulate_qemuimg_info(process, img, success, "rubbish");
    });

    MP_EXPECT_THROW_THAT(mp::backend::resize_instance_image(mp::MemorySize{"5G"}, img), std::runtime_error,
                         Property(&std::runtime_error::what, AllOf(HasSubstr("not"), HasSubstr("size"))));
}
