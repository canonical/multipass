#include "common.h"
#include "mock_sftp.h"
#include "mock_ssh.h"

#include <multipass/ssh/sftp_dir_iterator.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
auto get_dummy_attr(const char* name, uint8_t type)
{
    auto attr = static_cast<sftp_attributes_struct*>(calloc(1, sizeof(struct sftp_attributes_struct)));
    attr->name = strdup(name);
    attr->type = type;
    return attr;
}

auto get_dummy_dir(const char* name)
{
    auto dir = static_cast<sftp_dir_struct*>(calloc(1, sizeof(struct sftp_dir_struct)));
    dir->name = strdup(name);
    return dir;
}
} // namespace

TEST(SFTPDirIterator, success)
{
    std::vector<sftp_dir> dirs{
        get_dummy_dir("dir"),
        get_dummy_dir("dir/dir1"),
        get_dummy_dir("dir/dir1/dir2"),
        get_dummy_dir("dir/dir3"),
    };

    std::vector<sftp_attributes> entries{
        get_dummy_attr("file1", SSH_FILEXFER_TYPE_REGULAR),
        get_dummy_attr("dir1", SSH_FILEXFER_TYPE_DIRECTORY),
        get_dummy_attr("file2", SSH_FILEXFER_TYPE_REGULAR),
        get_dummy_attr("dir2", SSH_FILEXFER_TYPE_DIRECTORY),
        get_dummy_attr("file3", SSH_FILEXFER_TYPE_REGULAR),
        nullptr,
        get_dummy_attr(".", SSH_FILEXFER_TYPE_DIRECTORY),
        get_dummy_attr("..", SSH_FILEXFER_TYPE_DIRECTORY),
        get_dummy_attr("file4", SSH_FILEXFER_TYPE_REGULAR),
        get_dummy_attr("file5", SSH_FILEXFER_TYPE_REGULAR),
        nullptr,
        get_dummy_attr("dir3", SSH_FILEXFER_TYPE_DIRECTORY),
        get_dummy_attr("file6", SSH_FILEXFER_TYPE_REGULAR),
        nullptr,
    };

    auto open_dir = [&, i = 0](auto...) mutable { return i == (int)dirs.size() ? nullptr : dirs[i++]; };
    auto read_dir = [&, i = 0](auto...) mutable { return i == (int)entries.size() ? nullptr : entries[i++]; };

    REPLACE(sftp_opendir, open_dir);
    REPLACE(sftp_readdir, read_dir);
    REPLACE(sftp_dir_eof, [](auto...) { return true; });

    mp::SFTPDirIterator iter{nullptr, "dir"};

    std::vector<std::string> result;
    while (iter.hasNext())
        result.emplace_back(iter.next()->name);

    EXPECT_THAT(result,
                UnorderedElementsAre("dir/file1", "dir/dir1", "dir/dir1/file2", "dir/dir1/dir2", "dir/dir1/dir2/file3",
                                     "dir/dir1/file4", "dir/dir1/file5", "dir/dir3", "dir/dir3/file6"));
}

TEST(SFTPDirIterator, fail_opendir)
{
    REPLACE(sftp_opendir, [](auto...) { return nullptr; });
    REPLACE(ssh_get_error, [](auto...) { return "SFTP server: No such file"; });

    sftp_session_struct sftp{};
    MP_EXPECT_THROW_THAT(
        (mp::SFTPDirIterator{&sftp, "dir"}), std::runtime_error,
        mpt::match_what(StrEq("[sftp] cannot open remote directory 'dir': SFTP server: No such file")));
}

TEST(SFTPDirIterator, fail_readdir)
{
    REPLACE(sftp_opendir, [](auto, auto path) { return get_dummy_dir(path); });
    REPLACE(sftp_readdir, [](auto...) { return nullptr; });
    REPLACE(sftp_dir_eof, [](auto...) { return false; });
    REPLACE(ssh_get_error, [](auto...) { return "SFTP server: Permission denied"; });

    sftp_session_struct sftp{};
    MP_EXPECT_THROW_THAT(
        (mp::SFTPDirIterator{&sftp, "dir"}), std::runtime_error,
        mpt::match_what(StrEq("[sftp] cannot read remote directory 'dir': SFTP server: Permission denied")));
}
