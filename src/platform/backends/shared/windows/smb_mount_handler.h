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

#ifndef MULTIPASS_SMB_MOUNT_HANDLER_H
#define MULTIPASS_SMB_MOUNT_HANDLER_H

#include <multipass/mount_handler.h>
#include <multipass/path.h>

#include <vector>

namespace multipass
{
struct SmbManager
{
    virtual bool share_exists(const QString& share_name) const;
    virtual void create_share(const QString& share_name, const QString& source, const QString& user) const;
    virtual void remove_share(const QString& share_name) const;
};

class SmbMountHandler : public MountHandler
{
public:
    SmbMountHandler(VirtualMachine* vm,
                    const SSHKeyProvider* ssh_key_provider,
                    const std::string& target,
                    VMMount mount_spec,
                    const multipass::Path& cred_dir,
                    const SmbManager& smb_manager);
    ~SmbMountHandler() override;

    void activate_impl(ServerVariant server, std::chrono::milliseconds timeout) override;
    void deactivate_impl(bool force) override;
    bool is_active() override;

private:
    QString source;
    QString share_name;
    QDir cred_dir;
    std::vector<uint8_t> enc_key;
    const SmbManager* smb_manager;

    void remove_cred_files(const QString& user_id);
    void encrypt_credentials_to_file(const QString& cred_filename, const QString& iv_filename,
                                     const std::string& ptext);
    std::string decrypt_credentials_from_file(const QString& cred_filename, const QString& iv_filename);
};
} // namespace multipass
#endif // MULTIPASS_SMB_MOUNT_HANDLER_H
