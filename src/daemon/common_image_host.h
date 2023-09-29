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

#ifndef MULTIPASS_COMMON_IMAGE_HOST_H_
#define MULTIPASS_COMMON_IMAGE_HOST_H_

#include "multipass/vm_image_host.h"

#include <QStringList>
#include <QTimer>

#include <chrono>

namespace multipass
{

class CommonVMImageHost : public VMImageHost
{
public:
    void for_each_entry_do(const Action& action) final;
    VMImageInfo info_for_full_hash(const std::string& full_hash) final;
    void update_manifests(const bool is_force_update_from_network);

protected:
    void on_manifest_update_failure(const std::string& details);
    void on_manifest_empty(const std::string& details);
    void check_remote_is_supported(const std::string& remote_name) const;
    void check_alias_is_supported(const std::string& alias, const std::string& remote_name) const;
    bool alias_verifies_image_is_supported(const QStringList& aliases, const std::string& remote_name) const;

    virtual void for_each_entry_do_impl(const Action& action) = 0;
    virtual VMImageInfo info_for_full_hash_impl(const std::string& full_hash) = 0;
    virtual void clear() = 0;
    virtual void fetch_manifests(const bool is_force_update_from_network) = 0;
};

} // namespace multipass

#endif /* MULTIPASS_COMMON_IMAGE_HOST_H_ */
