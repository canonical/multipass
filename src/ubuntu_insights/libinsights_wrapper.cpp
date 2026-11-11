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

#include <ubuntu_insights/libinsights_wrapper.h>

namespace multipass
{
LibInsights::LibInsights(const Singleton<LibInsights>::PrivatePass& pass) noexcept
    : Singleton<LibInsights>{pass}
{
}

void LibInsights::insights_free(void* ptr) const
{
    ::insights_free(ptr);
}

char* LibInsights::insights_collect(insights_const_config* config,
                                    insights_const_char* source,
                                    insights_const_collect_flags* flags,
                                    char** out_report) const
{
    return ::insights_collect(config, source, flags, out_report);
}

char* LibInsights::insights_compile(insights_const_config* config,
                                    insights_const_compile_flags* flags,
                                    char** out_report) const
{
    return ::insights_compile(config, flags, out_report);
}

char* LibInsights::insights_write(insights_const_config* config,
                                  insights_const_char* source,
                                  insights_const_char* report,
                                  insights_const_write_flags* flags) const
{
    return ::insights_write(config, source, report, flags);
}

char* LibInsights::insights_upload(insights_const_config* config,
                                   insights_const_char** sources,
                                   size_t sources_len,
                                   insights_const_upload_flags* flags) const
{
    return ::insights_upload(config, sources, sources_len, flags);
}

insights_consent_state LibInsights::insights_get_consent_state(insights_const_config* config,
                                                               insights_const_char* source) const
{
    return ::insights_get_consent_state(config, source);
}

char* LibInsights::insights_set_consent_state(insights_const_config* config,
                                              insights_const_char* source,
                                              bool new_state) const
{
    return ::insights_set_consent_state(config, source, new_state);
}

bool LibInsights::insights_get_system_opt_out_state(insights_const_config* config) const
{
    return ::insights_get_system_opt_out_state(config);
}

char* LibInsights::insights_set_system_opt_out_state(insights_const_config* config,
                                                     bool state) const
{
    return ::insights_set_system_opt_out_state(config, state);
}

void LibInsights::insights_set_log_callback(insights_logger_callback callback) const
{
    ::insights_set_log_callback(callback);
}
} // namespace multipass
