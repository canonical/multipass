/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#include "default_vm_workflow_provider.h"

#include <multipass/query.h>

namespace mp = multipass;

mp::DefaultVMWorkflowProvider::DefaultVMWorkflowProvider(const QUrl& workflows_url, URLDownloader* downloader,
                                                         VMImageVault* image_vault)
    : workflows_url{workflows_url}, url_downloader{downloader}, image_vault{image_vault}
{
}

mp::VMImageInfo mp::DefaultVMWorkflowProvider::fetch_workflow(const Query& /* query */)
{
    return {};
}

void mp::DefaultVMWorkflowProvider::for_each_entry_do(const Action& /* action */)
{
}

std::vector<mp::VMImageInfo> mp::DefaultVMWorkflowProvider::all_workflows()
{
    return {};
}
