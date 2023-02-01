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

#ifndef MULTIPASS_ALIAS_DICT_H
#define MULTIPASS_ALIAS_DICT_H

#include <multipass/alias_definition.h>
#include <multipass/terminal.h>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace multipass
{
class AliasDict
{
public:
    typedef AliasMap DictType;
    typedef typename DictType::key_type key_type;
    typedef typename DictType::mapped_type mapped_type;
    typedef typename DictType::size_type size_type;

    AliasDict(Terminal* term);
    ~AliasDict();
    bool add_alias(const std::string& alias, const AliasDefinition& command);
    bool exists_alias(const std::string& alias);
    bool remove_alias(const std::string& alias);
    std::vector<std::string> remove_aliases_for_instance(const std::string& instance);
    std::optional<AliasDefinition> get_alias(const std::string& alias) const;
    DictType::iterator begin()
    {
        return aliases.begin();
    }
    DictType::iterator end()
    {
        return aliases.end();
    }
    DictType::const_iterator cbegin() const
    {
        return aliases.cbegin();
    }
    DictType::const_iterator cend() const
    {
        return aliases.cend();
    }
    bool empty() const
    {
        return aliases.empty();
    }
    size_type size() const
    {
        return aliases.size();
    }
    void clear()
    {
        if (!aliases.empty())
        {
            modified = true;

            aliases.clear();
        }
    }

private:
    void load_dict();
    void save_dict();

    bool modified = false;
    DictType aliases;
    std::string aliases_file;
    std::ostream& cout;
    std::ostream& cerr;
}; // class AliasDict
} // namespace multipass
#endif // MULTIPASS_ALIAS_DICT_H
