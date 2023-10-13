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

#ifndef MULTIPASS_VM_IMAGE_VAULT_H
#define MULTIPASS_VM_IMAGE_VAULT_H

#include "disabled_copy_move.h"
#include "fetch_type.h"
#include "memory_size.h"
#include "path.h"
#include "progress_monitor.h"
#include "vm_image_info.h"

#include <QDir>
#include <QFile>
#include <QString>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace multipass
{
class VMImageHost;
namespace vault
{
// Helper functions and classes for all image vault types
QString filename_for(const Path& path);
QString copy(const QString& file_name, const QDir& output_dir);
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
class VMImageVault : private DisabledCopyMove
{
public:
    using UPtr = std::unique_ptr<VMImageVault>;
    using PrepareAction = std::function<VMImage(const VMImage&)>;

    virtual ~VMImageVault() = default;
    virtual VMImage fetch_image(const FetchType& fetch_type,
                                const Query& query,
                                const PrepareAction& prepare,
                                const ProgressMonitor& monitor,
                                const bool unlock,
                                const std::optional<std::string>& checksum,
                                const Path& save_dir) = 0;
    virtual void remove(const std::string& name) = 0;
    virtual bool has_record_for(const std::string& name) = 0;
    virtual void prune_expired_images() = 0;
    virtual void update_images(const FetchType& fetch_type, const PrepareAction& prepare,
                               const ProgressMonitor& monitor) = 0;
    virtual MemorySize minimum_image_size_for(const std::string& id) = 0;
    virtual void clone(const std::string& source_instance_name, const std::string& dist_instance_name) = 0;
    virtual VMImageHost* image_host_for(const std::string& remote_name) const = 0;
    virtual std::vector<std::pair<std::string, VMImageInfo>> all_info_for(const Query& query) const = 0;

protected:
    VMImageVault() = default;
};
} // namespace multipass
#endif // MULTIPASS_VM_IMAGE_VAULT_H
