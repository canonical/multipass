#ifndef MOCK_SFTP_UTILS_H
#define MOCK_SFTP_UTILS_H

#include "mock_singleton_helpers.h"

#include <multipass/ssh/sftp_utils.h>

namespace multipass::test
{
struct MockSFTPUtils : public SFTPUtils
{
    using SFTPUtils::SFTPUtils;

    MOCK_METHOD(fs::path, get_local_file_target,
                (const fs::path& source_path, const fs::path& target_path, bool make_parent), (override));
    MOCK_METHOD(fs::path, get_remote_file_target,
                (sftp_session sftp, const fs::path& source_path, const fs::path& target_path, bool make_parent),
                (override));
    MOCK_METHOD(fs::path, get_local_dir_target,
                (const fs::path& source_path, const fs::path& target_path, bool make_parent), (override));
    MOCK_METHOD(fs::path, get_remote_dir_target,
                (sftp_session sftp, const fs::path& source_path, const fs::path& target_path, bool make_parent),
                (override));
    MOCK_METHOD(void, mkdir_recursive, (sftp_session sftp, const fs::path& path), (override));
    MOCK_METHOD(std::unique_ptr<SFTPDirIterator>, make_SFTPDirIterator, (sftp_session sftp, const fs::path& path),
                (override));
    MOCK_METHOD(std::unique_ptr<SFTPClient>, make_SFTPClient,
                (const std::string& host, int port, const std::string& username, const std::string& priv_key_blob),
                (override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockSFTPUtils, SFTPUtils);
};
} // namespace multipass::test

#endif // MOCK_SFTP_UTILS_H
