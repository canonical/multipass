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

#include <memory>

namespace mp = multipass;
namespace mpp = mp::platform;

namespace
{
const auto snapshot_template = QStringLiteral("@%1"); /* avoid colliding with suspension snapshots; prefix with a char
                                                         that can't be part of the name, to avoid confusion */

std::unique_ptr<mp::QemuImgProcessSpec> make_capture_spec(const QString& tag, const QString& image_path)
{
    return std::make_unique<mp::QemuImgProcessSpec>(QStringList{"snapshot", "-c", tag, image_path},
                                                    /* src_img = */ "", image_path);
}

std::unique_ptr<mp::QemuImgProcessSpec> make_restore_spec(const QString& tag, const QString& image_path)
{
    return std::make_unique<mp::QemuImgProcessSpec>(QStringList{"snapshot", "-a", tag, image_path},
                                                    /* src_img = */ "", image_path);
}
} // namespace

mp::QemuSnapshot::QemuSnapshot(const std::string& name, const std::string& comment,
                               std::shared_ptr<const Snapshot> parent, const VMSpecs& specs, const QString& image_path)
    : BaseSnapshot(name, comment, std::move(parent), specs), image_path{image_path}
{
}

mp::QemuSnapshot::QemuSnapshot(const QJsonObject& json, const mp::QemuVirtualMachine& vm) : BaseSnapshot(json, vm)
{
}

void mp::QemuSnapshot::capture_impl()
{
    auto tag = derive_tag();

    // Avoid creating more than one snapshot with the same tag (creation would succeed, but we'd then be unable to
    // identify the snapshot by tag)
    if (backend::instance_image_has_snapshot(image_path, tag.toUtf8()))
        throw std::runtime_error{fmt::format(
            "A snapshot with the same tag already exists in the image. Image: {}; tag: {})", image_path, tag)};

    auto process = mpp::make_process(make_capture_spec(tag, image_path));

    auto process_state = process->execute();
    if (!process_state.completed_successfully())
    {
        throw std::runtime_error(fmt::format("Internal error: qemu-img failed ({}) with output:\n{}",
                                             process_state.failure_message(), process->read_all_standard_error()));
    }
}

void mp::QemuSnapshot::erase_impl() // TODO@snapshots
{
    throw NotImplementedOnThisBackendException{"Snapshot erasing"};
}

void mp::QemuSnapshot::apply_impl() // TODO@ricab deduplicate
{
    auto process = mpp::make_process(make_restore_spec(derive_tag(), image_path));

    auto process_state = process->execute();
    if (!process_state.completed_successfully())
    {
        throw std::runtime_error(fmt::format("Internal error: qemu-img failed ({}) with output:\n{}",
                                             process_state.failure_message(), process->read_all_standard_error()));
    }
}

QString mp::QemuSnapshot::derive_tag() const
{
    return snapshot_template.arg(get_name().c_str());
}
