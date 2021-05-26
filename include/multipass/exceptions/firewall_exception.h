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

#ifndef MULTIPASS_FIREWALL_EXCEPTION_H
#define MULTIPASS_FIREWALL_EXCEPTION_H

#include <stdexcept>

namespace multipass
{
class FirewallException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

} // namespace multipass

#endif // MULTIPASS_FIREWALL_EXCEPTION_H
