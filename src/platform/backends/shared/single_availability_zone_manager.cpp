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

#include <multipass/file_ops.h>
#include <multipass/single_availability_zone_manager.h>
#include <multipass/utils.h>

namespace mp = multipass;

namespace
{
const mp::Subnet subnet_range{"10.97.0.0/20"};
constexpr auto subnet_prefix_length = 24;

mp::Subnet get_subnet(const mp::Path& data_dir)
{
    auto network_dir = MP_UTILS.make_dir(QDir(data_dir), "network");
    QFile subnet_file{network_dir + "/multipass_subnet"};
    MP_FILEOPS.open(subnet_file, QIODevice::ReadWrite | QIODevice::Text);
    if (MP_FILEOPS.size(subnet_file) > 0)
    {
        auto content = MP_FILEOPS.read_all(subnet_file).trimmed().toStdString();
        mp::Subnet{mp::IPAddress{content + ".0"}, subnet_prefix_length};
    }

    auto new_subnet = subnet_range.get_specific_subnet(1, subnet_prefix_length);
    auto content = new_subnet.address().as_string();
    content = content.substr(0, content.rfind('.'));
    MP_FILEOPS.write(subnet_file, content.data(), content.length());
    return new_subnet;
}
} // namespace

namespace multipass
{

SingleAvailabilityZoneManager::SingleAvailabilityZoneManager(const mp::Path& data_dir)
    // We name this zone "0" since that matches the naming of our bridge name from before the
    // introduction of AZs; see src/platform/backends/qemu/linux/qemu_platform_detail_linux.cpp.
    : zone{"0", get_subnet(data_dir)}
{
}

} // namespace multipass
