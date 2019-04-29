/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#ifndef MULTIPASS_CREATE_IMAGE_EXCEPTION
#define MULTIPASS_CREATE_IMAGE_EXCEPTION

#include <QException>

#include <string>

namespace multipass
{
class CreateImageException : public QException
{
public:
    CreateImageException(const std::string& err) : error_string{err}
    {
    }

    void raise() const override
    {
        throw *this;
    }
    CreateImageException* clone() const override
    {
        return new CreateImageException(*this);
    }
    const char* what() const noexcept
    {
        return error_string.c_str();
    }

private:
    std::string error_string;
};
} // namespace multipass
#endif // MULTIPASS_CREATE_IMAGE_EXCEPTION
