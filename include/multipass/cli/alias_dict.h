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

class QJsonObject;

namespace multipass
{
static constexpr auto default_context_name = "default";

typedef std::pair<std::string, std::string> ContextAliasPair;

// The alias dictionary is basically a mapping between strings and contexts. The string represents the context name
// and the associated context is itself a map relating alias names to alias definitions.
class AliasDict
{
public:
    typedef std::unordered_map<std::string, AliasContext> DictType;
    typedef typename DictType::key_type key_type;
    typedef typename DictType::mapped_type mapped_type;
    typedef typename DictType::size_type size_type;

    AliasDict(Terminal* term);
    ~AliasDict();
    void set_active_context(const std::string& new_active_context);
    std::string active_context_name() const;
    const AliasContext& get_active_context() const;
    bool add_alias(const std::string& alias, const AliasDefinition& command);
    bool exists_alias(const std::string& alias) const;
    bool is_alias_unique(const std::string& alias) const;
    bool remove_alias(const std::string& alias);
    bool remove_context(const std::string& context);
    std::vector<ContextAliasPair> remove_aliases_for_instance(const std::string& instance);
    std::optional<ContextAliasPair> get_context_and_alias(const std::string& alias) const;
    std::optional<AliasDefinition> get_alias_from_current_context(const std::string& alias) const;
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
        return (aliases.empty() || (aliases.size() == 1 && get_active_context().empty()));
    }
    size_type size() const
    {
        return aliases.size();
    }
    void clear()
    {
        if (!empty())
        {
            modified = true;

            aliases.clear();
            active_context = default_context_name;
            aliases[default_context_name] = AliasContext();
        }
    }
    QJsonObject to_json() const;

private:
    void load_dict();
    void save_dict();
    void sanitize_contexts();
    std::optional<AliasDefinition> get_alias_from_all_contexts(const std::string& alias) const;

    std::string active_context;
    DictType aliases;

    bool modified = false;
    std::string aliases_file;
    std::ostream& cout;
    std::ostream& cerr;
}; // class AliasDict
} // namespace multipass
#endif // MULTIPASS_ALIAS_DICT_H
