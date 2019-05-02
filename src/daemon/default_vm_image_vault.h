/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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

#ifndef MULTIPASS_DEFAULT_VM_IMAGE_VAULT_H
#define MULTIPASS_DEFAULT_VM_IMAGE_VAULT_H

#include <multipass/days.h>
#include <multipass/path.h>
#include <multipass/query.h>
#include <multipass/vm_image.h>
#include <multipass/vm_image_host.h>
#include <multipass/vm_image_vault.h>

#include <QDir>
#include <QFuture>

#include <memory>
#include <mutex>
#include <unordered_map>

namespace multipass
{
class URLDownloader;
class VMImageHost;
class VaultRecord
{
public:
    multipass::VMImage image;
    multipass::Query query;
    std::chrono::system_clock::time_point last_accessed;
};
class DefaultVMImageVault final : public VMImageVault
{
public:
    DefaultVMImageVault(std::vector<VMImageHost*> image_host, URLDownloader* downloader, multipass::Path cache_dir_path,
                        multipass::Path data_dir_path, multipass::days days_to_expire);
    VMImage fetch_image(const FetchType& fetch_type, const Query& query, const PrepareAction& prepare,
                        const ProgressMonitor& monitor) override;
    void remove(const std::string& name) override;
    bool has_record_for(const std::string& name) override;
    void prune_expired_images() override;
    void update_images(const FetchType& fetch_type, const PrepareAction& prepare,
                       const ProgressMonitor& monitor) override;

private:
    VMImage image_instance_from(const std::string& name, const VMImage& prepared_image);
    VMImage extract_image_from(const std::string& instance_name, const VMImage& source_image,
                               const ProgressMonitor& monitor);
    VMImage extract_downloaded_image(const VMImage& source_image, const ProgressMonitor& monitor);
    VMImage fetch_kernel_and_initrd(const VMImageInfo& info, const VMImage& source_image, const QDir& image_dir,
                                    const ProgressMonitor& monitor);
    VMImage finalize_image_records(const Query& query, const VMImage& prepared_image);
    VMImageInfo info_for(const Query& query);
    void persist_image_records();
    void persist_instance_records();

    std::vector<VMImageHost*> image_hosts;
    URLDownloader* const url_downloader;
    const QDir cache_dir;
    const QDir data_dir;
    const QDir instances_dir;
    const QDir images_dir;
    const days days_to_expire;
    std::mutex fetch_mutex;

    std::unordered_map<std::string, VaultRecord> prepared_image_records;
    std::unordered_map<std::string, VaultRecord> instance_image_records;
    std::unordered_map<std::string, VMImageHost*> remote_image_host_map;
    std::unordered_map<std::string, QFuture<VMImage>> in_progress_image_fetches;
};
}
#endif // MULTIPASS_DEFAULT_VM_IMAGE_VAULT_H
