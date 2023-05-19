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

#ifndef MULTIPASS_QEMU_SNAPSHOT_H
#define MULTIPASS_QEMU_SNAPSHOT_H

#include <shared/base_snapshot.h>

namespace multipass
{

class QemuSnapshot : public BaseSnapshot
{
public:
    QemuSnapshot(const std::string& name, const std::string& comment, std::shared_ptr<const Snapshot> parent,
                 const VMSpecs& specs, const QString& image_path);

    void capture() override;

private:
    QString image_path;
};

} // namespace multipass

#endif // MULTIPASS_QEMU_SNAPSHOT_H
