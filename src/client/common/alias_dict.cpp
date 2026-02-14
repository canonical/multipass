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

#include <multipass/cli/alias_dict.h>
#include <multipass/constants.h>
#include <multipass/file_ops.h>
#include <multipass/format.h>
#include <multipass/json_utils.h>
#include <multipass/logging/log.h>
#include <multipass/standard_paths.h>
#include <multipass/utils.h>

#include <QDir>
#include <QFile>
#include <QTemporaryFile>

namespace mp = multipass;
namespace mpl = multipass::logging;

mp::AliasDict::AliasDict(mp::Terminal* term) : cerr(term->cerr())
{
}

mp::AliasDict::AliasDict(mp::Terminal* term,
                         const std::string& active_context,
                         const std::filesystem::path& filename)
    : active_context(active_context),
      aliases{{active_context, AliasContext{}}},
      aliases_file(filename),
      cerr(term->cerr())
{
}

mp::AliasDict::~AliasDict()
{
    if (modified && !aliases_file.empty())
    {
        try
        {
            save_file();
        }
        catch (std::runtime_error& e)
        {
            cerr << fmt::format("Error saving aliases dictionary: {}\n", e.what());
        }
    }
}

std::filesystem::path mp::AliasDict::default_filename()
{
    const auto file_name = QStringLiteral("%1_aliases.json").arg(mp::client_name);
    const auto user_config_path =
        QDir{MP_STDPATHS.writableLocation(mp::StandardPaths::GenericConfigLocation)};
    const auto cli_client_dir_path = QDir{user_config_path.absoluteFilePath(mp::client_name)};

    return cli_client_dir_path.absoluteFilePath(file_name).toStdString();
}

mp::AliasDict mp::AliasDict::load_file(mp::Terminal* term, const std::filesystem::path& filename)
{
    if (auto filedata = MP_FILEOPS.try_read_file(filename))
    {
        try
        {
            auto json = boost::json::parse(*filedata);
            return value_to<AliasDict>(json, JSONContext{term, filename});
        }
        catch (const boost::system::system_error& e)
        {
            mpl::error("aliases", "Error parsing file '{}': {}", filename, e.what());
        }
    }
    return AliasDict(term, default_context_name, filename);
}

void mp::AliasDict::set_active_context(const std::string& new_active_context)
{
    if (new_active_context != active_context)
    {
        modified = true;
        active_context = new_active_context;
    }

    // When switching active context, make sure that a context associated with the new active
    // context exists.
    if (aliases.try_emplace(active_context, mp::AliasContext{}).second)
        modified = true;
}

std::string mp::AliasDict::active_context_name() const
{
    return active_context;
}

const mp::AliasContext& mp::AliasDict::get_active_context() const
{
    try
    {
        return aliases.at(active_context);
    }
    catch (const std::out_of_range&)
    {
        throw std::runtime_error(
            fmt::format("active context \"{}\" does not exist in dictionary", active_context));
    }
}

bool mp::AliasDict::add_alias(const std::string& alias, const mp::AliasDefinition& command)
{
    if (aliases[active_context].try_emplace(alias, command).second)
    {
        modified = true;

        return true;
    }

    return false;
}

bool mp::AliasDict::exists_alias(const std::string& alias) const
{
    for (const auto& [_, context_dict] : aliases)
    {
        if (context_dict.find(alias) != context_dict.cend())
        {
            return true;
        }
    }

    return false;
}

bool mp::AliasDict::is_alias_unique(const std::string& alias) const
{
    return get_alias_from_all_contexts(alias) ? true : false;
}

bool mp::AliasDict::remove_alias(const std::string& alias)
{
    if (aliases[active_context].erase(alias) > 0)
    {
        modified = true;
        return true;
    }

    return false;
}

bool mp::AliasDict::remove_context(const std::string& context)
{
    if (aliases.erase(context) > 0)
    {
        modified = true;

        if (active_context == context)
            set_active_context(default_context_name);

        return true;
    }

    return false;
}

// This function removes the context called as the instance, and then it iterates through all the
// remaining contexts to see if there are aliases defined for the instance.
std::vector<mp::ContextAliasPair> mp::AliasDict::remove_aliases_for_instance(
    const std::string& instance)
{
    std::vector<mp::ContextAliasPair> removed_aliases;

    remove_context(instance);

    const auto old_context_name = active_context;

    for (auto dict_it = begin(); dict_it != end(); ++dict_it)
    {
        for (auto context_it = dict_it->second.begin(); context_it != dict_it->second.end();
             ++context_it)
        {
            if (context_it->second.instance == instance)
            {
                removed_aliases.emplace_back(std::make_pair(dict_it->first, context_it->first));
            }
        }
    }

    for (auto [context_from, removed_alias] : removed_aliases)
    {
        set_active_context(context_from);
        remove_alias(removed_alias);
    }

    active_context = old_context_name;

    return removed_aliases;
}

// The argument is an alias name, which can have the forms (i) "alias" or (ii) "context.alias".
// (i): returns <active context name, alias name> if the alias exists in the current context;
//   std::nullopt otherwise.
// (ii): returns <context name, alias name> if the alias exists in the given
//   context; std::nullopt otherwise.
std::optional<mp::ContextAliasPair> mp::AliasDict::get_context_and_alias(
    const std::string& alias) const
{
    // This will never throw because we already checked that the active context exists.
    if (aliases.at(active_context).count(alias) > 0)
        return std::make_pair(active_context, alias);

    std::string::size_type dot_pos = alias.rfind('.');

    if (dot_pos == std::string::npos)
        return std::nullopt;

    std::string context = alias.substr(0, dot_pos);

    if (aliases.count(context) == 0)
        return std::nullopt;

    std::string alias_only = alias.substr(dot_pos + 1);

    return (aliases.at(context).count(alias_only) == 0
                ? std::nullopt
                : std::make_optional(std::make_pair(context, alias_only)));
}

std::optional<mp::AliasDefinition> mp::AliasDict::get_alias_from_current_context(
    const std::string& alias) const
{
    try
    {
        return aliases.at(active_context).at(alias);
    }
    catch (const std::out_of_range&)
    {
        return std::nullopt;
    }
}

// Returns an alias definition if:
// - the given alias exists in the current context, or
// - exists in another context and is unique, or
// - is fully qualified (that is, contains a dot) and exists.
// The given alias can be fully qualified or not.
std::optional<mp::AliasDefinition> mp::AliasDict::get_alias(const std::string& alias) const
{
    std::optional<mp::AliasDefinition> alias_in_current_context =
        get_alias_from_current_context(alias);

    if (alias_in_current_context)
        return alias_in_current_context;

    // If the alias is not on the current context, look for it in the rest of the contexts. But make
    // sure there is only one existing alias with that name.

    auto unique_alias_in_all_contexts = get_alias_from_all_contexts(alias);

    if (unique_alias_in_all_contexts)
        return unique_alias_in_all_contexts;

    // If the alias given was not found, then it is in the form "context.alias": the input string
    // must be split.

    // No dot, no alias.
    std::string::size_type dot_pos;
    if ((dot_pos = alias.find('.')) == std::string::npos)
        return std::nullopt;

    std::string context = alias.substr(0, dot_pos);
    std::string alias_only = alias.substr(dot_pos + 1);

    try
    {
        return aliases.at(context).at(alias_only);
    }
    catch (const std::out_of_range&)
    {
        return std::nullopt;
    }
}

void mp::AliasDict::save_file()
{
    sanitize_contexts();

    auto json = boost::json::value_from(*this);
    MP_FILEOPS.write_transactionally(QString::fromStdString(aliases_file.string()),
                                     pretty_print(json));
}

// This function removes the contexts which do not contain aliases, except the active context.
void mp::AliasDict::sanitize_contexts()
{
    // To avoid invalidating iterators, the function works in two stages. First, the aliases which
    // need to be removed are determined and, second, they are effectively removed.
    std::vector<std::string> empty_contexts;

    for (auto& context : aliases)
    {
        if (context.first != active_context && context.second.empty())
        {
            empty_contexts.push_back(context.first);
        }
    }

    if (!empty_contexts.empty())
    {
        modified = true;
        for (const auto& context : empty_contexts)
        {
            aliases.erase(context);
        }
    }
}

// Returns an alias definition iff the given alias name is unique across all the contexts. The given
// alias name cannot be fully qualified, that is, it must not be prepended by a context name.
std::optional<mp::AliasDefinition> mp::AliasDict::get_alias_from_all_contexts(
    const std::string& alias) const
{
    const AliasDefinition* ret;
    bool found{false};

    for (const auto& [_, context_dict] : aliases)
    {
        auto found_alias = context_dict.find(alias);

        if (found_alias != context_dict.cend())
        {
            if (found) // This means that the alias exists in more than one context.
            {
                return std::nullopt;
            }
            else
            {
                found = true;
                ret = &(found_alias->second);
            }
        }
    }

    return found ? std::make_optional(*ret) : std::nullopt;
}

void mp::tag_invoke(const boost::json::value_from_tag&,
                    boost::json::value& json,
                    const mp::AliasDict& alias_dict)
{
    json = {{"active-context", alias_dict.active_context},
            {"contexts", boost::json::value_from(alias_dict.aliases, SortJsonKeys{})}};
}

mp::AliasDict mp::tag_invoke(const boost::json::value_to_tag<mp::AliasDict>&,
                             const boost::json::value& json,
                             const mp::AliasDict::JSONContext& context)
{
    if (json.is_null())
        return AliasDict(context.term, default_context_name, context.filename);

    const auto& obj = json.as_object();
    if (obj.empty())
        return AliasDict(context.term, default_context_name, context.filename);

    AliasDict result(context.term);
    result.aliases_file = context.filename;
    // If the JSON does not contain the active-context field, then the file was written by a version
    // of Multipass previous than the introduction of alias contexts.
    if (obj.contains("active-context"))
    {
        result.active_context = value_to<std::string>(obj.at("active-context"));
        result.aliases = value_to<AliasDict::DictType>(obj.at("contexts"));
    }
    else
    {
        result.active_context = default_context_name;
        result.aliases = {{result.active_context, value_to<AliasContext>(json)}};
    }

    return result;
}
