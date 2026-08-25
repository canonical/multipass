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
#include "mock_libssh.h"

#include <multipass/ssh/plain_sftp_message.h>

#include <string_view>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct TestPlainSftpMessage : public Test
{
    constexpr static auto bad_addr = 0xdeadbeefdeadbeefull; // should reliably segfault on 32/64-bit

    sftp_client_message_struct raw_msg{};
    sftp_session fake_sftp = reinterpret_cast<sftp_session>(bad_addr);
    ssh_string fake_data = reinterpret_cast<ssh_string>(bad_addr);       // same type as below
    ssh_string fake_handle = reinterpret_cast<ssh_string>(bad_addr + 1); // + 1 makes it distinct

    mpt::MockLibssh::GuardedMock guarded_mock = mpt::MockLibssh::inject<NiceMock>();
    mpt::MockLibssh& mock_libssh = *guarded_mock.first;

    std::unique_ptr<mp::PlainSftpMessage> make_message()
    {
        // PlainSftpMessage takes ownership of raw_msg, but the freeing call is mocked and no-op by
        // default. Custom expectations on sftp_client_message_free mustn't free either.
        return std::make_unique<mp::PlainSftpMessage>(raw_msg);
    }
};
} // namespace

// Wire values are checked at compile time (see sftp_wire_compat.cpp)

TEST_F(TestPlainSftpMessage, destructorFreesRawMessage)
{
    EXPECT_CALL(mock_libssh, sftp_client_message_free(&raw_msg)).Times(1);

    [[maybe_unused]] const auto msg = make_message();
}

TEST_F(TestPlainSftpMessage, typeForwardsToLibssh)
{
    const auto msg = make_message();
    EXPECT_CALL(mock_libssh, sftp_client_message_get_type(&raw_msg))
        .WillOnce(Return(static_cast<uint8_t>(SSH_FXP_WRITE)));

    EXPECT_THAT(msg->type(), Eq(mp::SftpMessageType::write));
}

TEST_F(TestPlainSftpMessage, filenameEmptyWhenAbsent)
{
    const auto msg = make_message();
    EXPECT_CALL(mock_libssh, sftp_client_message_get_filename(&raw_msg)).WillOnce(Return(nullptr));

    EXPECT_THAT(msg->filename(), IsEmpty());
}

TEST_F(TestPlainSftpMessage, filenameForwardsToLibssh)
{
    const auto msg = make_message();
    const auto filename = "/some/path";

    EXPECT_CALL(mock_libssh, sftp_client_message_get_filename(&raw_msg)).WillOnce(Return(filename));

    const auto result = msg->filename();
    EXPECT_THAT(result, StrEq(filename));
    EXPECT_THAT(result.data(), Eq(filename)); // same address, no local var leak
}

TEST_F(TestPlainSftpMessage, dataEmptyWhenAbsent)
{
    raw_msg.data = nullptr;
    const auto msg = make_message();

    EXPECT_THAT(msg->data(), IsEmpty());
}

TEST_F(TestPlainSftpMessage, dataReadsBinarySafeContent)
{
    raw_msg.data = fake_data;
    const auto msg = make_message();

    // embed a NUL to prove this is length-bound, not C-string-bound
    static constexpr char raw[] = {'a', '\0', 'b'};
    EXPECT_CALL(mock_libssh, ssh_string_get_char(fake_data)).WillOnce(Return(raw));
    EXPECT_CALL(mock_libssh, ssh_string_len(fake_data)).WillOnce(Return(sizeof(raw)));

    EXPECT_THAT(msg->data(), Eq(std::string_view{raw, sizeof(raw)}));
}

TEST_F(TestPlainSftpMessage, submessageNulloptWhenAbsent)
{
    const auto msg = make_message();
    EXPECT_CALL(mock_libssh, sftp_client_message_get_submessage(&raw_msg))
        .WillOnce(Return(nullptr));

    EXPECT_THAT(msg->submessage(), Eq(std::nullopt));
}

TEST_F(TestPlainSftpMessage, submessageForwardsToLibssh)
{
    const auto msg = make_message();
    const auto extension = "posix-rename@openssh.com";

    EXPECT_CALL(mock_libssh, sftp_client_message_get_submessage(&raw_msg))
        .WillOnce(Return(extension));

    const auto result = msg->submessage();
    EXPECT_THAT(result, Optional(std::string_view{extension}));
    EXPECT_THAT(result->data(), Eq(extension)); // same address, no local var leak
}

TEST_F(TestPlainSftpMessage, flagsForwardsToLibssh)
{
    const auto msg = make_message();
    constexpr uint32_t raw_flags = mp::SftpOpenFlags::write | mp::SftpOpenFlags::creat;

    EXPECT_CALL(mock_libssh, sftp_client_message_get_flags(&raw_msg)).WillOnce(Return(raw_flags));

    EXPECT_THAT(msg->flags(), Eq(raw_flags));
}

TEST_F(TestPlainSftpMessage, offsetReadsRawField)
{
    raw_msg.offset = 123456789ull;
    const auto msg = make_message();

    EXPECT_THAT(msg->offset(), Eq(123456789ull));
}

TEST_F(TestPlainSftpMessage, lengthReadsRawField)
{
    raw_msg.len = 4096;
    const auto msg = make_message();

    EXPECT_THAT(msg->length(), Eq(4096u));
}

TEST_F(TestPlainSftpMessage, attributesNulloptWhenAbsent)
{
    raw_msg.attr = nullptr;
    const auto msg = make_message();

    EXPECT_THAT(msg->attributes(), Eq(std::nullopt));
}

TEST_F(TestPlainSftpMessage, attributesMapsSelectFields)
{
    sftp_attributes_struct raw_attr{};
    raw_attr.flags = mp::SftpAttrFlags::size | mp::SftpAttrFlags::uidgid;
    raw_attr.size = 42;
    raw_attr.uid = 1000;
    raw_attr.gid = 1000;
    raw_attr.permissions = 0644;
    raw_attr.atime = 111;
    raw_attr.mtime = 222;
    raw_msg.attr = &raw_attr;

    const auto msg = make_message();
    const auto attributes = msg->attributes();

    ASSERT_TRUE(attributes.has_value());
    EXPECT_THAT(attributes->flags, Eq(raw_attr.flags));
    EXPECT_THAT(attributes->size, Eq(raw_attr.size));
    EXPECT_THAT(attributes->uid, Eq(raw_attr.uid));
    EXPECT_THAT(attributes->gid, Eq(raw_attr.gid));
    EXPECT_THAT(attributes->permissions, Eq(raw_attr.permissions));
    EXPECT_THAT(attributes->atime, Eq(raw_attr.atime));
    EXPECT_THAT(attributes->mtime, Eq(raw_attr.mtime));
}

TEST_F(TestPlainSftpMessage, handleForwardsToLibssh)
{
    raw_msg.sftp = fake_sftp;
    raw_msg.handle = fake_handle;
    const auto msg = make_message();

    int registered_id = 7;
    EXPECT_CALL(mock_libssh, sftp_handle(fake_sftp, fake_handle)).WillOnce(Return(&registered_id));

    EXPECT_THAT(msg->handle(), Eq(static_cast<void*>(&registered_id)));
}

TEST_F(TestPlainSftpMessage, removeHandleRemovesResolvedHandle)
{
    raw_msg.sftp = fake_sftp;
    raw_msg.handle = fake_handle;
    const auto msg = make_message();

    int registered_id = 7;
    ON_CALL(mock_libssh, sftp_handle(fake_sftp, fake_handle)).WillByDefault(Return(&registered_id));
    EXPECT_CALL(mock_libssh, sftp_handle_remove(fake_sftp, &registered_id)).Times(1);

    msg->remove_handle();
}

TEST_F(TestPlainSftpMessage, removeHandleNoopWhenUnresolved)
{
    raw_msg.sftp = fake_sftp;
    raw_msg.handle = fake_handle;
    const auto msg = make_message();

    ON_CALL(mock_libssh, sftp_handle(fake_sftp, fake_handle)).WillByDefault(Return(nullptr));
    EXPECT_CALL(mock_libssh, sftp_handle_remove).Times(0);

    msg->remove_handle();
}

TEST_F(TestPlainSftpMessage, replyStatusForwardsToLibssh)
{
    const auto msg = make_message();

    const auto why = "because";
    EXPECT_CALL(mock_libssh,
                sftp_reply_status(&raw_msg,
                                  static_cast<uint32_t>(mp::SftpStatus::no_such_file),
                                  StrEq(why)))
        .WillOnce(Return(SSH_OK));

    EXPECT_TRUE(msg->reply_status(mp::SftpStatus::no_such_file, why));
}

TEST_F(TestPlainSftpMessage, replyStatusFailsOnLibsshError)
{
    const auto msg = make_message();
    EXPECT_CALL(mock_libssh, sftp_reply_status(&raw_msg, _, _)).WillOnce(Return(SSH_ERROR));

    EXPECT_FALSE(msg->reply_status(mp::SftpStatus::ok, "anything"));
}
