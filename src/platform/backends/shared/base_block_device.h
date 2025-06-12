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

#ifndef MULTIPASS_BASE_BLOCK_DEVICE_H
#define MULTIPASS_BASE_BLOCK_DEVICE_H

#include <multipass/block_device.h>
#include <multipass/memory_size.h>
#include <multipass/path.h>

#include <optional>
#include <string>

namespace multipass
{
class BaseBlockDevice : public BlockDevice
{
public:
    BaseBlockDevice(const std::string& name,
                    const Path& image_path,
                    const MemorySize& size,
                    const std::string& format = "qcow2",
                    const std::optional<std::string>& attached_vm = std::nullopt);

    // Core properties
    const std::string& name() const override;
    const Path& image_path() const override;
    const MemorySize& size() const override;
    const std::string& format() const override;
    const std::optional<std::string>& attached_vm() const override;

    // Operations
    void attach_to_vm(const std::string& vm_name) override;
    void detach_from_vm() override;
    void delete_device() override;

    // State queries
    bool is_attached() const override;
    bool exists() const override;

protected:
    // Virtual methods for backend-specific behavior
    virtual void remove_image_file();
    virtual void validate_attach(const std::string& vm_name);
    virtual void validate_detach();

private:
    std::string device_name;
    Path device_image_path;
    MemorySize device_size;
    std::string device_format;
    std::optional<std::string> device_attached_vm;
};
} // namespace multipass

#endif // MULTIPASS_BASE_BLOCK_DEVICE_H