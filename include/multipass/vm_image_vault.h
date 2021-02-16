/*
 * Copyright (C) 2017-2020 Canonical, Ltd.
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

#ifndef MULTIPASS_VM_IMAGE_VAULT_H
#define MULTIPASS_VM_IMAGE_VAULT_H

#include <multipass/fetch_type.h>
#include <multipass/memory_size.h>
#include <multipass/path.h>
#include <multipass/progress_monitor.h>

#include <QFile>
#include <QString>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace multipass
{
class VMImageHost;
namespace vault
{
// Helper functions and classes for all image vault types
QString filename_for(const Path& path);
void delete_file(const Path& path);
QString compute_image_hash(const Path& image_path);
void verify_image_download(const Path& image_path, const QString& image_hash);
QString extract_image(const Path& image_path, const ProgressMonitor& monitor, const bool delete_file = false);
std::unordered_map<std::string, VMImageHost*> configure_image_host_map(const std::vector<VMImageHost*>& image_hosts);

class DeleteOnException
{
public:
    explicit DeleteOnException(const Path& path) : file(path)
    {
    }
    ~DeleteOnException()
    {
        if (std::uncaught_exceptions() > initial_exc_count)
        {
            file.remove();
        }
    }

private:
    QFile file;
    const int initial_exc_count = std::uncaught_exceptions();
};
} // namespace vault

class Query;
class VMImage;
class VMImageVault
{
public:
    using UPtr = std::unique_ptr<VMImageVault>;
    using PrepareAction = std::function<VMImage(const VMImage&)>;

    virtual ~VMImageVault() = default;
    virtual VMImage fetch_image(const FetchType& fetch_type, const Query& query, const PrepareAction& prepare,
                                const ProgressMonitor& monitor) = 0;
    virtual void remove(const std::string& name) = 0;
    virtual bool has_record_for(const std::string& name) = 0;
    virtual void prune_expired_images() = 0;
    virtual void update_images(const FetchType& fetch_type, const PrepareAction& prepare,
                               const ProgressMonitor& monitor) = 0;
    virtual MemorySize minimum_image_size_for(const std::string& id) = 0;
    virtual VMImageHost* image_host_for(const std::string& remote_name) = 0;

protected:
    VMImageVault() = default;
    VMImageVault(const VMImageVault&) = delete;
    VMImageVault& operator=(const VMImageVault&) = delete;
};
} // namespace multipass
#endif // MULTIPASS_VM_IMAGE_VAULT_H
