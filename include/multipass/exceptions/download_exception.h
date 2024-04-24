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

#ifndef MULTIPASS_DOWNLOAD_H
#define MULTIPASS_DOWNLOAD_H

#include <fmt/format.h>

#include <QException>
#include <string>

namespace multipass
{
class DownloadException : public QException
{
public:
    DownloadException(const std::string& url, const std::string& cause)
        : error_string{fmt::format("failed to download from '{}': {}", url, cause)}
    {
    }
    void raise() const override
    {
        throw *this;
    }
    DownloadException* clone() const override
    {
        return new DownloadException(*this);
    }
    const char* what() const noexcept override
    {
        return error_string.c_str();
    }

private:
    std::string error_string;
};
} // namespace multipass
#endif // MULTIPASS_DOWNLOAD_H
