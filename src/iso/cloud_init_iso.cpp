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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include <multipass/cloud_init_iso.h>
#include <multipass/file_ops.h>
#include <multipass/format.h>
#include <multipass/yaml_node_utils.h>

#include <QFile>

#include <array>
#include <cctype>

namespace mp = multipass;
namespace mpu = multipass::utils;

// ISO9660 + Joliet Extension format
// ---------------------------------
// 32KB Reserved
// ---------------------------------
// Primary Volume Descriptor
// ---------------------------------
// Supplemental Volume Descriptor (Joliet extension)
// ---------------------------------
// Volume Descriptor Set Terminator
// ---------------------------------
// Path Tables pointing to dir records
// ---------------------------------
// "ISO9660 records"
// root directory record
// root parent directory record
// file record 1
// ...
// file record N
// ---------------------------------
// "Joliet" version of the same records but with UCS-2 character names for dirs/files
// root directory record
// root parent directory record
// file record 1
// ...
// file record N
// ---------------------------------
// data blocks
// ---------------------------------

namespace
{
constexpr auto logical_block_size = 2048u;

constexpr std::uint16_t operator"" _u16(unsigned long long v)
{
    return static_cast<std::uint16_t>(v);
}

constexpr std::uint8_t operator"" _u8(unsigned long long v)
{
    return static_cast<std::uint8_t>(v);
}

uint8_t to_u8(uint32_t value)
{
    return static_cast<uint8_t>(value);
}

uint32_t to_u32(uint8_t value)
{
    return static_cast<uint32_t>(value);
}

std::array<uint8_t, 8> to_lsb_msb(uint32_t value)
{
    return {{to_u8(value),
             to_u8(value >> 8u),
             to_u8(value >> 16u),
             to_u8(value >> 24u),
             to_u8(value >> 24u),
             to_u8(value >> 16u),
             to_u8(value >> 8u),
             to_u8(value)}};
}

std::array<uint8_t, 4> to_lsb_msb(uint16_t value)
{
    return {{to_u8(value), to_u8(value >> 8u), to_u8(value >> 8u), to_u8(value)}};
}

std::array<uint8_t, 4> to_lsb(uint32_t value)
{
    return {{to_u8(value), to_u8(value >> 8u), to_u8(value >> 16u), to_u8(value >> 24u)}};
}

bool is_system_little_endian()
{
    uint32_t test_value = 1; // In memory: 01 00 00 00 (little endian) or 00 00 00 01 (big endian)
    return *(reinterpret_cast<uint8_t*>(&test_value)) == 1; // Check the first byte
}

// std::array<uint8_t, 8> -> std::span<uint8_t, 8> when c++20 arrives
uint32_t from_lsb_msb(const std::array<uint8_t, 8>& bytes)
{
    // replace the is_system_little_endian() function with std::endian::native == std::endian::little when C++20 arrives
    return is_system_little_endian()
               ? to_u32(bytes[0]) | to_u32(bytes[1]) << 8u | to_u32(bytes[2]) << 16u | to_u32(bytes[3]) << 24u
               : to_u32(bytes[4]) << 24u | to_u32(bytes[5]) << 16u | to_u32(bytes[6]) << 8u | to_u32(bytes[7]);
}

template <typename T, typename SizeType, typename V>
void set_at(T& t, SizeType offset, const V& value)
{
    std::copy(std::begin(value), std::end(value), t.begin() + offset);
}

template <typename T>
void write(const T& t, QFile& f)
{
    f.write(reinterpret_cast<const char*>(t.data.data()), t.data.size());
}

// The below three utility functions should serve as the abstraction layer for binary file reading, the
// std::vector<uint8_t>, std::array<uint8_t, N> and uint8_t should be the only ones to receive data because they
// indicate the nature of the data which is raw binary bytes.
std::vector<uint8_t> read_bytes_to_vec(std::ifstream& file, std::streampos pos, size_t size)
{
    std::vector<uint8_t> buffer(size);
    file.seekg(pos);

    if (!MP_FILEOPS.read(file, reinterpret_cast<char*>(buffer.data()), size))
    {
        throw std::runtime_error(fmt::format("Can not read {} bytes data from file at {}.", size, std::streamoff(pos)));
    }

    return buffer;
}

template <size_t N>
std::array<uint8_t, N> read_bytes_to_array(std::ifstream& file, std::streampos pos)
{
    std::array<uint8_t, N> buffer{};
    file.seekg(pos);
    if (!MP_FILEOPS.read(file, reinterpret_cast<char*>(buffer.data()), N))
    {
        throw std::runtime_error(fmt::format("Can not read {} bytes data from file at {}.", N, std::streamoff(pos)));
    }

    return buffer;
}

uint8_t read_single_byte(std::ifstream& file, std::streampos pos)
{
    uint8_t data{};
    file.seekg(pos);
    if (!MP_FILEOPS.read(file, reinterpret_cast<char*>(&data), 1))
    {
        throw std::runtime_error(fmt::format("Can not read the next byte data from file at {}.", std::streamoff(pos)));
    }

    return data;
}

template <size_t size>
struct PaddedString
{
    PaddedString() : data(size, ' ')
    {
    }

    explicit PaddedString(const std::string& value) : PaddedString()
    {
        data.replace(0, value.size(), value);
    }

    auto begin() const
    {
        return data.begin();
    }
    auto end() const
    {
        return data.end();
    }

    std::string data;
};

template <size_t size>
struct U16PaddedString
{
    U16PaddedString() : data(size, '\0')
    {
        for (size_t i = 1; i < size; i += 2)
        {
            data.at(i) = ' ';
        }
    }

    explicit U16PaddedString(const std::string& value) : U16PaddedString()
    {
        auto it = value.begin();
        for (size_t i = 1; i < size; i += 2)
        {
            if (it == value.end())
                break;
            data.at(i) = *it;
            ++it;
        }
    }

    auto begin() const
    {
        return data.begin();
    }
    auto end() const
    {
        return data.end();
    }

    std::string data;
};

struct DecDateTime
{
    DecDateTime() : data{}
    {
        std::fill_n(data.begin(), 16, '0');
    }

    std::array<uint8_t, 17> data;
};

struct RootDirRecord
{
    enum class Type
    {
        root,
        root_parent
    };

    RootDirRecord(Type type, uint32_t location) : data{}
    {
        data[0] = data.size();
        set_at(data, 2, to_lsb_msb(location));

        uint32_t block_size = logical_block_size;
        set_at(data, 10, to_lsb_msb(block_size));    // size of entry (limited to one block)
        data[25] = 0x02;                             // record is a directory entry
        set_at(data, 28, to_lsb_msb(1_u16));         // vol seq #
        data[32] = 1;                                // id_len length
        data[33] = type == Type::root ? 0_u8 : 1_u8; // directory id
    }

    std::array<uint8_t, 34> data;
};

struct VolumeDescriptor
{
    VolumeDescriptor() : data{}
    {
        set_at(data, 1, std::string("CD001")); // identifier
        data[6] = 0x01;                        // volume descriptor version
    }

    void set_volume_size(uint32_t num_blocks)
    {
        set_at(data, 80, to_lsb_msb(num_blocks));
    }

    void set_root_dir_record(const RootDirRecord& record)
    {
        set_at(data, 156, record.data);
    }

    void set_path_table_info(uint32_t size, uint32_t location)
    {
        set_at(data, 132, to_lsb_msb(size));
        set_at(data, 140, to_lsb(location));
    }

    void set_common_fields()
    {
        uint16_t block_size = logical_block_size;
        set_at(data, 120, to_lsb_msb(1_u16));      // number of disks
        set_at(data, 124, to_lsb_msb(1_u16));      // disk number
        set_at(data, 128, to_lsb_msb(block_size)); // logical block size

        DecDateTime no_date;
        set_at(data, 813, no_date.data); // vol creation date-time
        set_at(data, 830, no_date.data); // vol modification date-time
        set_at(data, 847, no_date.data); // vol expiration date-time
        set_at(data, 864, no_date.data); // vol effective date-time

        data[881] = 0x01; // file structure version
    }

    std::array<uint8_t, logical_block_size> data;
};

struct VolumeDescriptorSetTerminator : VolumeDescriptor
{
    VolumeDescriptorSetTerminator()
    {
        data[0] = 0xFF; // Terminator Type
    }
};

struct PrimaryVolumeDescriptor : VolumeDescriptor
{
    PrimaryVolumeDescriptor()
    {
        data[0] = 0x01; // primary volume descriptor type

        set_at(data, 8, PaddedString<32>());          // System identifier
        set_at(data, 40, PaddedString<32>("cidata")); // volume identifier
        set_at(data, 190, PaddedString<623>());       // various ascii identifiers

        set_common_fields();
    }
};

struct JolietVolumeDescriptor : VolumeDescriptor
{
    JolietVolumeDescriptor()
    {
        data[0] = 0x02; // supplementary volume descriptor type

        set_at(data, 8, U16PaddedString<32>());          // System identifier
        set_at(data, 40, U16PaddedString<32>("cidata")); // volume identifier
        set_at(data, 190, U16PaddedString<623>());       // various UCS-2 identifiers

        set_at(data, 88, std::array<uint8_t, 3>{{0x25, 0x2f, 0x45}}); // Joliet UCS-2 escape sequence

        set_common_fields();
    }
};

bool is_even(size_t size)
{
    return (size % 2) == 0;
}

struct FileRecord
{
    FileRecord() : data(33, 0u)
    {
    }

    void set_fields(const std::string& name, uint32_t content_location, uint32_t size)
    {
        set_at(data, 2, to_lsb_msb(content_location)); // content block location
        set_at(data, 10, to_lsb_msb(size));            // size of extent
        data[25] = 0x00;                               // record is a file entry
        set_at(data, 28, to_lsb_msb(1_u16));           // vol seq #

        auto id_len = static_cast<uint8_t>(name.length());
        data[32] = id_len;
        auto pad_length = is_even(id_len) ? 1u : 0u;
        data.resize(data.size() + id_len + pad_length);
        set_at(data, 33, name);

        data[0] = static_cast<uint8_t>(data.size());
    }

    std::vector<uint8_t> data;
};

auto make_iso_name(const std::string& name)
{
    std::string iso_name{name};
    std::transform(iso_name.begin(), iso_name.end(), iso_name.begin(), [](unsigned char c) { return std::toupper(c); });
    std::transform(iso_name.begin(), iso_name.end(), iso_name.begin(),
                   [](unsigned char c) { return std::isalnum(c) ? c : '_'; });
    if (iso_name.size() > 8)
        iso_name = iso_name.substr(0, 8);
    iso_name.append(".;1");
    return iso_name;
}

struct ISOFileRecord : FileRecord
{
    ISOFileRecord(const std::string& name, uint32_t content_location, uint32_t size)
    {
        set_fields(make_iso_name(name), content_location, size);
    }
};

auto make_u16_name(const std::string& name)
{
    std::string u16_name(name.size() * 2u, '\0');
    const auto size = u16_name.size();
    auto it = name.begin();
    for (size_t i = 1; i < size; i += 2)
    {
        u16_name.at(i) = *it;
        ++it;
    }
    return u16_name;
}

std::string convert_u16_name_back(const std::string_view u16_name)
{
    if (!is_even(u16_name.size()))
    {
        throw std::runtime_error(
            fmt::format("The size of {} is not even, which does not conform to u16 name format.", u16_name.data()));
    }

    std::string original_name(u16_name.size() / 2, '\0');
    for (size_t i = 0; i < original_name.size(); ++i)
    {
        original_name[i] = u16_name[i * 2 + 1];
    }

    return original_name;
}

struct JolietFileRecord : FileRecord
{
    JolietFileRecord(const std::string& name, uint32_t content_location, uint32_t size)
    {
        set_fields(make_u16_name(name), content_location, size);
    }
};

struct RootPathTable
{
    explicit RootPathTable(uint32_t dir_record_location)
    {
        data[0] = 0x1; // dir id len (root id len is 1)
        set_at(data, 2, to_lsb(dir_record_location));
        data[6] = 0x01; // dir # of parent dir
        data[8] = 0x00; // dir id (0x00 = root)
    }
    std::array<uint8_t, 10> data{};
};

template <typename Size>
Size num_blocks(Size num_bytes)
{
    return ((num_bytes + logical_block_size - 1) / logical_block_size);
}

void seek_to_next_block(QFile& f)
{
    const auto next_block_pos = num_blocks(f.pos()) * logical_block_size;
    f.seek(next_block_pos);
}

void pad_to_end(QFile& f)
{
    seek_to_next_block(f);
    f.seek(f.pos() - 1);
    char end = 0;
    f.write(&end, 1);
}
} // namespace

void mp::CloudInitIso::add_file(const std::string& name, const std::string& data)
{
    files.push_back(FileEntry{name, data});
}

bool mp::CloudInitIso::contains(const std::string& name) const
{
    return std::find_if(files.cbegin(), files.cend(), [name](const FileEntry& file_entry) -> bool {
               return file_entry.name == name;
           }) != std::cend(files);
}

const std::string& mp::CloudInitIso::at(const std::string& name) const
{
    if (auto iter = std::find_if(files.cbegin(),
                                 files.cend(),
                                 [name](const FileEntry& file_entry) -> bool { return file_entry.name == name; });
        iter == std::cend(files))
    {
        throw std::runtime_error(fmt::format("Did not find the target file {} in the CloudInitIso instance.", name));
    }
    else
    {
        return iter->data;
    }
}

std::string& mp::CloudInitIso::at(const std::string& name)
{
    return const_cast<std::string&>(const_cast<const mp::CloudInitIso*>(this)->at(name));
}

std::string& mp::CloudInitIso::operator[](const std::string& name)
{
    if (auto iter = std::find_if(files.begin(),
                                 files.end(),
                                 [name](const FileEntry& file_entry) -> bool { return file_entry.name == name; });
        iter == std::end(files))
    {
        return files.emplace_back(FileEntry{name, std::string()}).data;
    }
    else
    {
        return iter->data;
    }
}

bool mp::CloudInitIso::erase(const std::string& name)
{
    if (auto iter = std::find_if(files.cbegin(),
                                 files.cend(),
                                 [name](const FileEntry& file_entry) -> bool { return file_entry.name == name; });
        iter != std::cend((files)))
    {
        files.erase(iter);
        return true;
    }

    return false;
}

void mp::CloudInitIso::write_to(const Path& path)
{
    QFile f{path};
    if (!f.open(QIODevice::WriteOnly))
        throw std::runtime_error{fmt::format(
            "Failed to open file for writing during cloud-init generation: {}; path: {}", f.errorString(), path)};

    const uint32_t num_reserved_bytes = 32768u;
    const uint32_t num_reserved_blocks = num_blocks(num_reserved_bytes);
    f.seek(num_reserved_bytes);

    PrimaryVolumeDescriptor prim_desc;
    JolietVolumeDescriptor joliet_desc;

    const uint32_t num_blocks_for_descriptors = 3u;
    const uint32_t num_blocks_for_path_table = 2u;
    const uint32_t num_blocks_for_dir_records = 2u;

    auto volume_size =
        num_reserved_blocks + num_blocks_for_descriptors + num_blocks_for_path_table + num_blocks_for_dir_records;
    for (const auto& entry : files)
    {
        volume_size += num_blocks(entry.data.size());
    }

    prim_desc.set_volume_size(volume_size);
    joliet_desc.set_volume_size(volume_size);

    uint32_t current_block_index = num_reserved_blocks + num_blocks_for_descriptors;

    // The following records are simply to specify that a root filesystem exists
    RootPathTable root_path{current_block_index + num_blocks_for_path_table};
    prim_desc.set_path_table_info(root_path.data.size(), current_block_index);
    ++current_block_index;

    RootPathTable joliet_root_path{current_block_index + num_blocks_for_path_table};
    joliet_desc.set_path_table_info(joliet_root_path.data.size(), current_block_index);
    ++current_block_index;

    RootDirRecord root_record{RootDirRecord::Type::root, current_block_index};
    RootDirRecord root_parent_record{RootDirRecord::Type::root_parent, current_block_index};
    prim_desc.set_root_dir_record(root_record);
    ++current_block_index;

    RootDirRecord joliet_root_record{RootDirRecord::Type::root, current_block_index};
    RootDirRecord joliet_root_parent_record{RootDirRecord::Type::root_parent, current_block_index};
    joliet_desc.set_root_dir_record(joliet_root_record);
    ++current_block_index;

    std::vector<ISOFileRecord> iso_file_records;
    std::vector<JolietFileRecord> joliet_file_records;

    for (const auto& entry : files)
    {
        iso_file_records.emplace_back(entry.name, current_block_index, entry.data.size());
        joliet_file_records.emplace_back(entry.name, current_block_index, entry.data.size());
        current_block_index += num_blocks(entry.data.size());
    }

    write(prim_desc, f);
    write(joliet_desc, f);
    write(VolumeDescriptorSetTerminator(), f);

    write(root_path, f);
    seek_to_next_block(f);
    write(joliet_root_path, f);
    seek_to_next_block(f);

    write(root_record, f);
    write(root_parent_record, f);
    for (const auto& iso_record : iso_file_records)
    {
        write(iso_record, f);
    }
    seek_to_next_block(f);

    write(joliet_root_record, f);
    write(joliet_root_parent_record, f);
    for (const auto& joliet_record : joliet_file_records)
    {
        write(joliet_record, f);
    }
    seek_to_next_block(f);

    for (const auto& entry : files)
    {
        f.write(entry.data.data(), entry.data.size());
        seek_to_next_block(f);
    }
    pad_to_end(f);
}

void mp::CloudInitIso::read_from(const std::filesystem::path& fs_path)
{
    // Please refer to the cloud_Init_Iso_read_me.md file for the preliminaries and the thought process of the
    // implementation
    std::ifstream iso_file{fs_path, std::ios_base::in | std::ios::binary};
    if (!MP_FILEOPS.is_open(iso_file))
    {
        throw std::runtime_error{
            fmt::format(R"("Failed to open file "{}" for reading: {}.")", fs_path.string(), strerror(errno))};
    }

    const uint32_t num_reserved_bytes = 32768u; // 16 data blocks, 32kb
    const uint32_t joliet_des_start_pos = num_reserved_bytes + sizeof(PrimaryVolumeDescriptor);
    if (read_single_byte(iso_file, joliet_des_start_pos) != 2_u8)
    {
        throw std::runtime_error("The Joliet volume descriptor is not in place.");
    }

    const std::array<uint8_t, 5> volume_identifier = read_bytes_to_array<5>(iso_file, joliet_des_start_pos + 1u);
    if (std::string_view(reinterpret_cast<const char*>(volume_identifier.data()), volume_identifier.size()) != "CD001")
    {
        throw std::runtime_error("The Joliet descriptor is malformed.");
    }

    const uint32_t root_dir_record_data_start_pos = joliet_des_start_pos + 156u;
    const std::array<uint8_t, 34> root_dir_record_data =
        read_bytes_to_array<34>(iso_file, root_dir_record_data_start_pos);
    // size of the data should 34, record is a directory entry and directory is a root directory instead of a root
    // parent directory
    if (root_dir_record_data[0] != 34_u8 || root_dir_record_data[25] != 2_u8 || root_dir_record_data[33] != 0_u8)
    {
        throw std::runtime_error("The root directory record data is malformed.");
    }

    // Use std::span when C++20 arrives to avoid the copy of the std::array<uint8_t, 8>
    std::array<uint8_t, 8> root_dir_record_data_location_lsb_bytes;
    // location lsb_msb bytes starts from 2
    std::copy_n(root_dir_record_data.cbegin() + 2u, 8, root_dir_record_data_location_lsb_bytes.begin());
    const uint32_t root_dir_record_data_location_by_blocks = from_lsb_msb(root_dir_record_data_location_lsb_bytes);
    const uint32_t file_records_start_pos = root_dir_record_data_location_by_blocks * logical_block_size +
                                            2u * sizeof(RootDirRecord); // total size of root dir and root dir parent

    uint32_t current_file_record_start_pos = file_records_start_pos;
    while (true)
    {
        const uint8_t file_record_data_size = read_single_byte(iso_file, current_file_record_start_pos);
        if (file_record_data_size == 0_u8)
        {
            break;
        }

        // In each iteration, the file record provides the size and location of the extent. Initially, we utilize this
        // information to navigate to and read the file data. Subsequently, we return to the file record to extract the
        // file name.
        const uint32_t file_content_location_by_blocks =
            from_lsb_msb(read_bytes_to_array<8>(iso_file, current_file_record_start_pos + 2u));
        const uint32_t file_content_size =
            from_lsb_msb(read_bytes_to_array<8>(iso_file, current_file_record_start_pos + 10u));
        const std::vector<uint8_t> file_content =
            read_bytes_to_vec(iso_file, file_content_location_by_blocks * logical_block_size, file_content_size);

        const uint32_t file_name_length_start_pos = current_file_record_start_pos + 32u;
        const uint8_t encoded_file_name_length = read_single_byte(iso_file, file_name_length_start_pos);
        const uint32_t file_name_start_pos = file_name_length_start_pos + 1u;
        const std::vector<uint8_t> encoded_file_name =
            read_bytes_to_vec(iso_file, file_name_start_pos, to_u32(encoded_file_name_length));

        const std::string orginal_file_name = convert_u16_name_back(
            std::string_view{reinterpret_cast<const char*>(encoded_file_name.data()), encoded_file_name.size()});
        files.emplace_back(FileEntry{orginal_file_name, std::string{file_content.cbegin(), file_content.cend()}});

        current_file_record_start_pos += to_u32(file_record_data_size);
    }
}

mp::CloudInitFileOps::CloudInitFileOps(const Singleton<CloudInitFileOps>::PrivatePass& pass) noexcept
    : Singleton<CloudInitFileOps>::Singleton{pass}
{
}

void mp::CloudInitFileOps::update_cloud_init_with_new_extra_interfaces_and_new_id(
    const std::string& default_mac_addr,
    const std::vector<NetworkInterface>& extra_interfaces,
    const std::string& new_instance_id,
    const std::filesystem::path& cloud_init_path) const
{
    CloudInitIso iso_file;
    iso_file.read_from(cloud_init_path);

    std::string& meta_data_file_content = iso_file.at("meta-data");
    meta_data_file_content =
        mpu::emit_cloud_config(mpu::make_cloud_init_meta_config_with_id_tweak(meta_data_file_content, new_instance_id));

    if (extra_interfaces.empty())
    {
        iso_file.erase("network-config");
    }
    else
    {
        // overwrite the whole network-config file content
        iso_file["network-config"] =
            mpu::emit_cloud_config(mpu::make_cloud_init_network_config(default_mac_addr, extra_interfaces));
    }
    iso_file.write_to(QString::fromStdString(cloud_init_path.string()));
}

void mp::CloudInitFileOps::add_extra_interface_to_cloud_init(const std::string& default_mac_addr,
                                                             const NetworkInterface& extra_interface,
                                                             const std::filesystem::path& cloud_init_path) const
{
    CloudInitIso iso_file;
    iso_file.read_from(cloud_init_path);
    std::string& meta_data_file_content = iso_file.at("meta-data");
    meta_data_file_content =
        mpu::emit_cloud_config(mpu::make_cloud_init_meta_config_with_id_tweak(meta_data_file_content));

    iso_file["network-config"] = mpu::emit_cloud_config(
        mpu::add_extra_interface_to_network_config(default_mac_addr, extra_interface, iso_file["network-config"]));
    iso_file.write_to(QString::fromStdString(cloud_init_path.string()));
}

std::string mp::CloudInitFileOps::get_instance_id_from_cloud_init(const std::filesystem::path& cloud_init_path) const
{
    CloudInitIso iso_file;
    iso_file.read_from(cloud_init_path);
    const std::string& meta_data_file_content = iso_file.at("meta-data");
    const auto meta_data_node = YAML::Load(meta_data_file_content);

    return meta_data_node["instance-id"].as<std::string>();
}
