/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#ifndef MULTIPASS_JSON_OUTPUT
#define MULTIPASS_JSON_OUTPUT

#include <multipass/cli/formatted_output.h>

namespace multipass
{
class JsonOutput final : public FormattedOutput
{
public:
    std::string process_info(InfoReply& info) const override;
    std::string process_list(ListReply& list) const override;
};
}
#endif // MULTIPASS_JSON_OUTPUT
