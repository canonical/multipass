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
    const auto file_name = fmt::format("{}_aliases.json", mp::client_name);
    const std::filesystem::path user_config_path{
        MP_STDPATHS.writableLocation(mp::StandardPaths::GenericConfigLocation).toStdString()};

    return absolute(user_config_path / mp::client_name / file_name);
}

mp::AliasDict mp::AliasDict::load_file(mp::Terminal* term, const std::filesystem::path& filename)
{
    if (auto filedata = MP_FILEOPS.try_read_file(filename))
    {
        try
        {
            auto json = boost::json::parse(*filedata);
            auto result = value_to<AliasDict>(json, JSONContext{term, filename});
            const auto previous_context = result.active_context_name();
            result.set_active_context(default_context_name);
            if (result.modified)
            {
                // Commands copy this dictionary; do not leave stale copies pending a save.
                result.save_file();
                result.modified = false;
            }
            if (previous_context != default_context_name)
                result.cerr << fmt::format(
                    "Warning: alias context '{}' is inactive. Only the default context is active. "
                    "Use `multipass aliases` to inspect retained definitions and "
                    "`multipass alias <instance>:<command> <new-name>` to recreate them.\n",
                    previous_context);
            return result;
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

bool mp::AliasDict::is_context_active(const std::string& context) const
{
    return context == default_context_name;
}

bool mp::AliasDict::is_inactive_alias(const std::string& alias) const
{
    if (get_alias(alias))
        return false;

    for (const auto& [context_name, context] : aliases)
    {
        if (is_context_active(context_name))
            continue;

        const auto prefix = context_name + ".";
        if (context.contains(alias) ||
            (alias.starts_with(prefix) && context.contains(alias.substr(prefix.size()))))
            return true;
    }

    return false;
}

bool mp::AliasDict::exists_alias(const std::string& alias) const
{
    for (const auto& [_, context_dict] : aliases)
    {
        if (context_dict.contains(alias))
        {
            return true;
        }
    }

    return false;
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
    if (aliases.at(active_context).contains(alias))
        return std::make_pair(active_context, alias);

    for (auto dot_pos = alias.rfind('.'); dot_pos != std::string::npos;
         dot_pos = dot_pos == 0 ? std::string::npos : alias.rfind('.', dot_pos - 1))
    {
        const auto context = aliases.find(alias.substr(0, dot_pos));
        const auto alias_only = alias.substr(dot_pos + 1);
        if (context != aliases.end() && context->second.contains(alias_only))
            return std::make_pair(context->first, alias_only);
    }

    return std::nullopt;
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

std::optional<mp::AliasDefinition> mp::AliasDict::get_alias(const std::string& alias) const
{
    const auto context = aliases.find(default_context_name);
    if (context == aliases.end())
        return std::nullopt;

    if (const auto found = context->second.find(alias); found != context->second.end())
        return found->second;

    const auto prefix = std::string{default_context_name} + ".";
    if (alias.starts_with(prefix))
    {
        if (const auto found = context->second.find(alias.substr(prefix.size()));
            found != context->second.end())
            return found->second;
    }

    return std::nullopt;
}

void mp::AliasDict::save_file()
{
    sanitize_contexts();

    auto json = boost::json::value_from(*this);
    MP_FILEOPS.write_transactionally(aliases_file, pretty_print(json));
}

// This function removes the contexts which do not contain aliases, except the active context.
void mp::AliasDict::sanitize_contexts()
{
    const auto removed = std::erase_if(aliases, [this](const auto& context) {
        return context.first != active_context && context.second.empty();
    });

    if (removed > 0)
        modified = true;
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
