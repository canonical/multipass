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

#pragma once

#include <boost/json.hpp>

#include <string>

namespace multipass
{

struct NewReleaseInfo
{
    std::string version;
    std::string url;
    std::string title;
    std::string description;
};

NewReleaseInfo tag_invoke(const boost::json::value_to_tag<NewReleaseInfo>&,
                          const boost::json::value& json);

} // namespace multipass
