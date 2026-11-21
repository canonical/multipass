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

#include <multipass/singleton.h>

// cgo generates C99 _Bool, which is not valid in C++.
#define _Bool bool
#include <insights/insights.h>
#undef _Bool

#define MP_LIBINSIGHTS multipass::LibInsights::instance()

namespace multipass
{
class LibInsights : public Singleton<LibInsights>
{
public:
    LibInsights(const Singleton<LibInsights>::PrivatePass& pass) noexcept;

    virtual void insights_free(void* ptr) const;
    [[nodiscard]] virtual char* insights_collect(insights_const_config* config,
                                                 insights_const_char* source,
                                                 insights_const_collect_flags* flags,
                                                 char** out_report) const;
    [[nodiscard]] virtual char* insights_compile(insights_const_config* config,
                                                 insights_const_compile_flags* flags,
                                                 char** out_report) const;
    [[nodiscard]] virtual char* insights_write(insights_const_config* config,
                                               insights_const_char* source,
                                               insights_const_char* report,
                                               insights_const_write_flags* flags) const;
    [[nodiscard]] virtual char* insights_upload(insights_const_config* config,
                                                insights_const_char** sources,
                                                size_t sources_len,
                                                insights_const_upload_flags* flags) const;
    [[nodiscard]] virtual insights_consent_state
    insights_get_consent_state(insights_const_config* config, insights_const_char* source) const;
    [[nodiscard]] virtual char* insights_set_consent_state(insights_const_config* config,
                                                           insights_const_char* source,
                                                           bool new_state) const;
    [[nodiscard]] virtual bool insights_get_system_opt_out_state(
        insights_const_config* config) const;
    [[nodiscard]] virtual char* insights_set_system_opt_out_state(insights_const_config* config,
                                                                  bool state) const;
    virtual void insights_set_log_callback(insights_logger_callback callback) const;
};
} // namespace multipass
