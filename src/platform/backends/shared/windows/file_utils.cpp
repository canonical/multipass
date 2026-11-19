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

#include "file_utils.h"
#include "file_io_api.h"

#include <multipass/logging/log.h>

#include <fmt/std.h>

#include <winioctl.h>

#include <memory>
#include <type_traits>

namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "windows-file-utils";

struct HandleCloser
{
    void operator()(HANDLE h) const noexcept
    {
        (void)MP_FILEIOAPI.CloseHandle(h);
    }
};

using UniqueHandle = std::unique_ptr<std::remove_pointer_t<HANDLE>, HandleCloser>;

std::error_code last_error(std::string_view operation, const std::filesystem::path& path)
{
    const std::error_code ec{static_cast<int>(MP_FILEIOAPI.GetLastError()), std::system_category()};
    mpl::error(category,
               "clear_sparse_flag(...) > {} failed for {}: {}",
               operation,
               path,
               ec.message());
    return ec;
}
} // namespace

std::error_code multipass::clear_sparse_flag(const std::filesystem::path& path)
{
    mpl::debug(category, "clear_sparse_flag(...) > path: {}", path);
    const auto path_w = path.wstring();

    // Only touch the file when it is actually sparse. This avoids failing on volumes without
    // sparse support and leaves the file alone otherwise.
    const auto attributes = MP_FILEIOAPI.GetFileAttributesW(path_w.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
        return last_error("GetFileAttributesW", path);

    if (!(attributes & FILE_ATTRIBUTE_SPARSE_FILE))
        return {};

    const auto raw_handle = MP_FILEIOAPI.CreateFileW(path_w.c_str(),
                                                     GENERIC_WRITE,
                                                     FILE_SHARE_READ,
                                                     nullptr,
                                                     OPEN_EXISTING,
                                                     0,
                                                     nullptr);
    if (raw_handle == INVALID_HANDLE_VALUE)
        return last_error("CreateFileW", path);

    const UniqueHandle file{raw_handle};

    FILE_SET_SPARSE_BUFFER sparse_buffer{.SetSparse = FALSE};
    DWORD bytes_returned{0};
    if (!MP_FILEIOAPI.DeviceIoControl(file.get(),
                                      FSCTL_SET_SPARSE,
                                      &sparse_buffer,
                                      sizeof(sparse_buffer),
                                      nullptr,
                                      0,
                                      &bytes_returned,
                                      nullptr))
        return last_error("DeviceIoControl(FSCTL_SET_SPARSE)", path);

    return {};
}
