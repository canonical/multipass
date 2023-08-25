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

#include "qemu_snapshot.h"
#include "qemu_virtual_machine.h"
#include "shared/qemu_img_utils/qemu_img_utils.h"

#include <multipass/platform.h>
#include <multipass/process/qemuimg_process_spec.h>
#include <multipass/top_catch_all.h>
#include <multipass/virtual_machine_description.h>

#include <scope_guard.hpp>

#include <memory>

namespace mp = multipass;
namespace mpp = mp::platform;

namespace
{
const auto snapshot_template = QStringLiteral("@%1"); /* avoid colliding with suspension snapshots; prefix with a char
                                                         that can't be part of the name, to avoid confusion */

std::unique_ptr<mp::QemuImgProcessSpec> make_capture_spec(const QString& tag, const mp::Path& image_path)
{
    return std::make_unique<mp::QemuImgProcessSpec>(QStringList{"snapshot", "-c", tag, image_path},
                                                    /* src_img = */ "", image_path);
}

std::unique_ptr<mp::QemuImgProcessSpec> make_restore_spec(const QString& tag, const mp::Path& image_path)
{
    return std::make_unique<mp::QemuImgProcessSpec>(QStringList{"snapshot", "-a", tag, image_path},
                                                    /* src_img = */ "", image_path);
}

std::unique_ptr<mp::QemuImgProcessSpec> make_delete_spec(const QString& tag, const mp::Path& image_path)
{
    return std::make_unique<mp::QemuImgProcessSpec>(QStringList{"snapshot", "-d", tag, image_path},
                                                    /* src_img = */ "", image_path);
}

void checked_exec_qemu_img(std::unique_ptr<mp::QemuImgProcessSpec> spec)
{
    auto process = mpp::make_process(std::move(spec));

    auto process_state = process->execute();
    if (!process_state.completed_successfully())
    {
        throw std::runtime_error(fmt::format("Internal error: qemu-img failed ({}) with output:\n{}",
                                             process_state.failure_message(), process->read_all_standard_error()));
    }
}
} // namespace

mp::QemuSnapshot::QemuSnapshot(const std::string& name, const std::string& comment, const VMSpecs& specs,
                               std::shared_ptr<Snapshot> parent, VirtualMachineDescription& desc)
    : BaseSnapshot(name, comment, specs, std::move(parent)), desc{desc}, image_path{desc.image.image_path}
{
}

mp::QemuSnapshot::QemuSnapshot(const QJsonObject& json, QemuVirtualMachine& vm, VirtualMachineDescription& desc)
    : BaseSnapshot(json, vm), desc{desc}, image_path{desc.image.image_path}
{
}

void mp::QemuSnapshot::capture_impl()
{
    auto tag = derive_tag();

    // Avoid creating more than one snapshot with the same tag (creation would succeed, but we'd then be unable to
    // identify the snapshot by tag)
    if (backend::instance_image_has_snapshot(image_path, tag))
        throw std::runtime_error{fmt::format(
            "A snapshot with the same tag already exists in the image. Image: {}; tag: {})", image_path, tag)};

    checked_exec_qemu_img(make_capture_spec(tag, image_path));
}

void mp::QemuSnapshot::erase_impl()
{
    checked_exec_qemu_img(make_delete_spec(derive_tag(), image_path));
}

void mp::QemuSnapshot::apply_impl()
{
    auto rollback = sg::make_scope_guard(
        [this, old_desc = desc]() noexcept { top_catch_all(get_name(), [this, &old_desc]() { desc = old_desc; }); });

    desc.num_cores = get_num_cores();
    desc.mem_size = get_mem_size();
    desc.disk_space = get_disk_space();

    checked_exec_qemu_img(make_restore_spec(derive_tag(), image_path));
    rollback.dismiss();
}

QString mp::QemuSnapshot::derive_tag() const
{
    return snapshot_template.arg(get_name().c_str());
}