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
#include <multipass/user_messages/alias_context_deprecation_warning.h>
#include <multipass/utils.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
// TODO@remove-prefer remove along with alias contexts
std::optional<std::string> legacy_active_context(const boost::json::value& json)
{
    const auto* active_context =
        json.is_object() ? json.as_object().if_contains("active-context") : nullptr;
    if (active_context && active_context->is_string() &&
        active_context->as_string() != mp::default_context_name)
        return std::string{active_context->as_string()};

    return std::nullopt;
}
} // namespace

mp::AliasDict::AliasDict(mp::Terminal* term) : cerr(term->cerr())
{
}

mp::AliasDict::AliasDict(mp::Terminal* term, const std::filesystem::path& filename)
    : aliases{{default_context_name, AliasContext{}}}, aliases_file(filename), cerr(term->cerr())
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
            result.migrate_legacy_file(json); // TODO@remove-prefer remove with alias contexts
            return result;
        }
        catch (const boost::system::system_error& e)
        {
            mpl::error("aliases", "Error parsing file '{}': {}", filename, e.what());
        }
    }
    return AliasDict(term, filename);
}

const mp::AliasContext& mp::AliasDict::get_active_context() const
{
    // The default context is always present, see the constructors and clear.
    return aliases.at(default_context_name);
}

bool mp::AliasDict::add_alias(const std::string& alias, const mp::AliasDefinition& command)
{
    if (aliases[default_context_name].try_emplace(alias, command).second)
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
    return remove_alias(default_context_name, alias);
}

bool mp::AliasDict::remove_alias(const std::string& context, const std::string& alias)
{
    if (const auto it = aliases.find(context); it != aliases.end() && it->second.erase(alias) > 0)
    {
        modified = true;
        return true;
    }

    return false;
}

// Removes the aliases of the given instance from all the contexts.
std::vector<mp::ContextAliasPair> mp::AliasDict::remove_aliases_for_instance(
    const std::string& instance)
{
    std::vector<mp::ContextAliasPair> removed_aliases;

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

    for (const auto& [context_from, removed_alias] : removed_aliases)
        remove_alias(context_from, removed_alias);

    return removed_aliases;
}

// The argument is an alias name, which can have the forms (i) "alias" or (ii) "context.alias".
// (i): returns <default context name, alias name> if the alias exists in the default context;
//   std::nullopt otherwise.
// (ii): returns <context name, alias name> if the alias exists in the given
//   context; std::nullopt otherwise.
std::optional<mp::ContextAliasPair> mp::AliasDict::get_context_and_alias(
    const std::string& alias) const
{
    if (get_active_context().contains(alias))
        return std::make_pair(std::string{default_context_name}, alias);

    // TODO@remove-prefer only needed for dotted aliases in inactive contexts
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
    const auto& context = get_active_context();
    if (const auto found = context.find(alias); found != context.end())
        return found->second;

    return std::nullopt;
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

// TODO@remove-prefer remove along with alias contexts
// Warn about a legacy active context and persist the normalized dictionary once, so that it is
// dropped. A failed save is not fatal: the original file is left intact and the migration is
// retried on the next load.
void mp::AliasDict::migrate_legacy_file(const boost::json::value& json)
{
    const auto previous_context = legacy_active_context(json);
    if (!previous_context)
        return;

    cerr << make_inactive_alias_context_warning(*previous_context);

    try
    {
        save_file();
    }
    catch (const std::runtime_error& e)
    {
        mpl::warn("aliases", "Could not migrate aliases file '{}': {}", aliases_file, e.what());
    }

    // Commands hold copies of this dictionary; none of them should save it again.
    modified = false;
}

// This function removes the contexts which do not contain aliases, except the default context.
void mp::AliasDict::sanitize_contexts()
{
    const auto removed = std::erase_if(aliases, [](const auto& context) {
        return context.first != default_context_name && context.second.empty();
    });

    if (removed > 0)
        modified = true;
}

void mp::tag_invoke(const boost::json::value_from_tag&,
                    boost::json::value& json,
                    const mp::AliasDict& alias_dict)
{
    // Older versions of Multipass require this field.
    json = {{"active-context", default_context_name},
            {"contexts", boost::json::value_from(alias_dict.aliases, SortJsonKeys{})}};
}

mp::AliasDict mp::tag_invoke(const boost::json::value_to_tag<mp::AliasDict>&,
                             const boost::json::value& json,
                             const mp::AliasDict::JSONContext& context)
{
    if (json.is_null())
        return AliasDict(context.term, context.filename);

    const auto& obj = json.as_object();
    if (obj.empty())
        return AliasDict(context.term, context.filename);

    AliasDict result(context.term);
    result.aliases_file = context.filename;
    // If the JSON does not contain the active-context field, then the file was written by a version
    // of Multipass previous than the introduction of alias contexts. Otherwise, the active-context
    // field is ignored, since only the default context can be active.
    if (obj.contains("active-context"))
        result.aliases = value_to<AliasDict::DictType>(obj.at("contexts"));
    else
        result.aliases = {{default_context_name, value_to<AliasContext>(json)}};

    // Normalize in memory without marking this dictionary or its copies for saving.
    result.aliases.try_emplace(default_context_name);

    return result;
}
