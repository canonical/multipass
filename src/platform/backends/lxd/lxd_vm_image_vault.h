/*
 * Copyright (C) 2020 Canonical, Ltd.
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

#ifndef MULTIPASS_LXD_VM_IMAGE_VAULT_H
#define MULTIPASS_LXD_VM_IMAGE_VAULT_H

#include <multipass/days.h>
#include <multipass/query.h>
#include <multipass/vm_image_host.h>
#include <multipass/vm_image_vault.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>

namespace multipass
{
class NetworkAccessManager;
class URLDownloader;

class LXDVMImageVault final : public VMImageVault
{
public:
    using TaskCompleteAction = std::function<void(const QJsonObject&)>;

    LXDVMImageVault(std::vector<VMImageHost*> image_host, URLDownloader* downloader, NetworkAccessManager* manager,
                    const QUrl& base_url, const QString& cache_dir_path, const multipass::days& days_to_expire);

    VMImage fetch_image(const FetchType& fetch_type, const Query& query, const PrepareAction& prepare,
                        const ProgressMonitor& monitor) override;
    void remove(const std::string& name) override;
    bool has_record_for(const std::string& name) override;
    void prune_expired_images() override;
    void update_images(const FetchType& fetch_type, const PrepareAction& prepare,
                       const ProgressMonitor& monitor) override;
    MemorySize minimum_image_size_for(const std::string& id) override;

private:
    VMImageInfo info_for(const Query& query);
    void lxd_download_image(const QString& id, const QString& stream_location, const Query& query,
                            const ProgressMonitor& monitor, const QString& last_used = QString());
    void url_download_image(const VMImageInfo& info, const QString& image_path, const ProgressMonitor& monitor);
    void poll_download_operation(const QJsonObject& json_reply, const ProgressMonitor& monitor);
    std::string lxd_import_metadata_and_image(const QString& metadata_path, const QString& image_path);
    std::string get_lxd_image_hash_for(const QString& id);
    QJsonArray retrieve_image_list();

    std::vector<VMImageHost*> image_hosts;
    URLDownloader* const url_downloader;
    NetworkAccessManager* manager;
    const QUrl base_url;
    const QString template_path;
    const days days_to_expire;
};
} // namespace multipass
#endif // MULTIPASS_LXD_VM_IMAGE_VAULT_H
