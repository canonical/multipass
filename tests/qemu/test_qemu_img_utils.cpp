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

#include "tests/common.h"
#include "tests/mock_process_factory.h"

#include <src/platform/backends/shared/qemu_img_utils/qemu_img_utils.h>

#include <multipass/constants.h>
#include <multipass/memory_size.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
const auto success = mp::ProcessState{0, std::nullopt};
const auto failure = mp::ProcessState{1, std::nullopt};
const auto crash = mp::ProcessState{std::nullopt, mp::ProcessState::Error{QProcess::Crashed, "core dumped"}};
const auto null_string_matcher = static_cast<std::optional<decltype(_)>>(std::nullopt);

using ImageConversionParamType =
    std::tuple<const char*, const char*, mp::ProcessState, bool, mp::ProcessState, std::optional<Matcher<std::string>>>;

void simulate_qemuimg_info_with_json(const mpt::MockProcess* process, const QString& expect_img,
                                     const mp::ProcessState& produce_result, const QByteArray& produce_output = {})
{
    ASSERT_EQ(process->program().toStdString(), "qemu-img");

    const auto args = process->arguments();
    ASSERT_EQ(args.size(), 3);

    EXPECT_EQ(args.at(0), "info");
    EXPECT_EQ(args.at(1), "--output=json");
    EXPECT_EQ(args.at(2), expect_img);

    InSequence s;

    EXPECT_CALL(*process, execute).WillOnce(Return(produce_result));
    if (produce_result.completed_successfully())
        EXPECT_CALL(*process, read_all_standard_output).WillOnce(Return(produce_output));
    else if (produce_result.exit_code)
        EXPECT_CALL(*process, read_all_standard_error).WillOnce(Return(produce_output));
    else
        ON_CALL(*process, read_all_standard_error).WillByDefault(Return(produce_output));
}

void simulate_qemuimg_resize(mpt::MockProcess* process, const QString& expect_img, const mp::MemorySize& expect_size,
                             const mp::ProcessState& produce_result)
{
    ASSERT_EQ(process->program().toStdString(), "qemu-img");

    const auto args = process->arguments();
    ASSERT_EQ(args.size(), 3);

    EXPECT_EQ(args.at(0).toStdString(), "resize");
    EXPECT_EQ(args.at(1), expect_img);
    EXPECT_THAT(args.at(2),
                ResultOf([](const auto& val) { return mp::MemorySize{val.toStdString()}; }, Eq(expect_size)));

    EXPECT_CALL(*process, execute(mp::image_resize_timeout)).Times(1).WillOnce(Return(produce_result));
}

void simulate_qemuimg_convert(const mpt::MockProcess* process, const QString& img_path,
                              const QString& expected_img_path, const mp::ProcessState& produce_result)
{
    ASSERT_EQ(process->program().toStdString(), "qemu-img");

    const auto args = process->arguments();
    ASSERT_EQ(args.size(), 6);

    EXPECT_EQ(args.at(0), "convert");
    EXPECT_EQ(args.at(1), "-p");
    EXPECT_EQ(args.at(2), "-O");
    EXPECT_EQ(args.at(3), "qcow2");
    EXPECT_EQ(args.at(4), img_path);
    EXPECT_EQ(args.at(5), expected_img_path);

    EXPECT_CALL(*process, execute).WillOnce(Return(produce_result));
}

template <class Matcher>
void test_image_resizing(const char* img, const mp::MemorySize& img_virtual_size, const mp::MemorySize& requested_size,
                         const mp::ProcessState& qemuimg_resize_result, std::optional<Matcher> throw_msg_matcher)
{
    auto process_count = 0;
    auto mock_factory_scope = mpt::MockProcessFactory::Inject();

    mock_factory_scope->register_callback([&](mpt::MockProcess* process) {
        ASSERT_LE(++process_count, 1);
        simulate_qemuimg_resize(process, img, requested_size, qemuimg_resize_result);
    });

    if (throw_msg_matcher)
        MP_EXPECT_THROW_THAT(mp::backend::resize_instance_image(requested_size, img), std::runtime_error,
                             mpt::match_what(*throw_msg_matcher));
    else
        mp::backend::resize_instance_image(requested_size, img);

    EXPECT_EQ(process_count, 1);
}

template <class Matcher>
void test_image_conversion(const char* img_path, const char* expected_img_path, const char* qemuimg_info_output,
                           const mp::ProcessState& qemuimg_info_result, bool attempt_convert,
                           const mp::ProcessState& qemuimg_convert_result, std::optional<Matcher> throw_msg_matcher)
{
    auto process_count = 0;
    auto mock_factory_scope = mpt::MockProcessFactory::Inject();
    const auto expected_final_process_count = attempt_convert ? 2 : 1;

    mock_factory_scope->register_callback([&](mpt::MockProcess* process) {
        ASSERT_LE(++process_count, expected_final_process_count);
        if (process_count == 1)
        {
            auto msg = QByteArray{qemuimg_info_output};

            simulate_qemuimg_info_with_json(process, img_path, qemuimg_info_result, msg);
        }
        else
        {
            simulate_qemuimg_convert(process, img_path, expected_img_path, qemuimg_convert_result);
        }
    });

    if (throw_msg_matcher)
        MP_EXPECT_THROW_THAT(mp::backend::convert_to_qcow_if_necessary(img_path), std::runtime_error,
                             mpt::match_what(*throw_msg_matcher));
    else
        EXPECT_THAT(mp::backend::convert_to_qcow_if_necessary(img_path), Eq(expected_img_path));

    EXPECT_EQ(process_count, expected_final_process_count);
}

struct ImageConversionTestSuite : public TestWithParam<ImageConversionParamType>
{
};

const std::vector<ImageConversionParamType> image_conversion_inputs{
    {"/fake/img/path", "{\n    \"format\": \"qcow2\"\n}", success, false, mp::ProcessState{}, null_string_matcher},
    {"/fake/img/path.qcow2", "{\n    \"format\": \"raw\"\n}", success, true, success, null_string_matcher},
    {"/fake/img/path.qcow2", "not found", failure, false, mp::ProcessState{},
     std::make_optional(HasSubstr("not found"))},
    {"/fake/img/path.qcow2", "{\n    \"format\": \"raw\"\n}", success, true, failure,
     std::make_optional(HasSubstr("qemu-img failed"))}};
} // namespace

TEST(QemuImgUtils, image_resizing_checks_minimum_size_and_proceeds_when_larger)
{
    const auto img = "/fake/img/path";
    const auto min_size = mp::MemorySize{"1G"};
    const auto request_size = mp::MemorySize{"3G"};
    const auto qemuimg_resize_result = success;
    const auto throw_msg_matcher = null_string_matcher;

    test_image_resizing(img, min_size, request_size, qemuimg_resize_result, throw_msg_matcher);
}

TEST(QemuImgUtils, image_resizing_checks_minimum_size_and_proceeds_when_equal)
{
    const auto img = "/fake/img/path";
    const auto min_size = mp::MemorySize{"1234554321"};
    const auto request_size = min_size;
    const auto qemuimg_resize_result = success;
    const auto throw_msg_matcher = null_string_matcher;

    test_image_resizing(img, min_size, request_size, qemuimg_resize_result, throw_msg_matcher);
}

TEST(QemuImgUtils, image_resize_detects_resizing_exit_failure_and_throws)
{
    const auto img = "imagine";
    const auto min_size = mp::MemorySize{"100M"};
    const auto request_size = mp::MemorySize{"400M"};
    const auto qemuimg_resize_result = failure;
    const auto throw_msg_matcher = std::make_optional(HasSubstr("qemu-img failed"));

    test_image_resizing(img, min_size, request_size, qemuimg_resize_result, throw_msg_matcher);
}

TEST(QemuImgUtils, image_resize_detects_resizing_crash_failure_and_throws)
{
    const auto img = "ubuntu";
    const auto min_size = mp::MemorySize{"100M"};
    const auto request_size = mp::MemorySize{"400M"};
    const auto qemuimg_resize_result = crash;
    const auto throw_msg_matcher =
        std::make_optional(AllOf(HasSubstr("qemu-img failed"), HasSubstr(crash.failure_message().toStdString())));

    test_image_resizing(img, min_size, request_size, qemuimg_resize_result, throw_msg_matcher);
}

TEST_P(ImageConversionTestSuite, properly_handles_image_conversion)
{
    const auto img_path = "/fake/img/path";
    const auto& [expected_img_path, qemuimg_info_output, qemuimg_info_result, attempt_convert, qemuimg_convert_result,
                 throw_msg_matcher] = GetParam();

    test_image_conversion(img_path, expected_img_path, qemuimg_info_output, qemuimg_info_result, attempt_convert,
                          qemuimg_convert_result, throw_msg_matcher);
}

INSTANTIATE_TEST_SUITE_P(QemuImgUtils, ImageConversionTestSuite, ValuesIn(image_conversion_inputs));
