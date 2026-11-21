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

#include <gtest/gtest.h>

#include "temp_dir.h"

#include <filesystem>

namespace multipass::test
{
class LibInsights : public testing::Test
{
protected:
    const char* to_const_char_ptr(const char8_t* data)
    {
        return reinterpret_cast<const char*>(data);
    }

    void SetUp() override
    {
        std::filesystem::path tempdir_fs = tempdir;

        consent_dir = (tempdir_fs / "consent").u8string();
        insights_dir = (tempdir_fs / "insights").u8string();
        system_config_dir = (tempdir_fs / "system").u8string();

        config = {
            .consent_dir = to_const_char_ptr(consent_dir.c_str()),
            .insights_dir = to_const_char_ptr(insights_dir.c_str()),
            .system_config_dir = to_const_char_ptr(system_config_dir.c_str()),
            .verbose = false,
        };
    }

    TempDir tempdir;
    std::u8string consent_dir;
    std::u8string insights_dir;
    std::u8string system_config_dir;
    insights_config config{};
};

TEST_F(LibInsights, missingConsentIsUnknown)
{
    EXPECT_EQ(MP_LIBINSIGHTS.insights_get_consent_state(&config, "multipass"),
              INSIGHTS_CONSENT_UNKNOWN);
}

TEST_F(LibInsights, consentCanBeCreatedAndUpdated)
{
    for (const auto consent : {true, false})
    {
        char* error = MP_LIBINSIGHTS.insights_set_consent_state(&config, "multipass", consent);

        EXPECT_EQ(error, nullptr) << error;
        EXPECT_EQ(MP_LIBINSIGHTS.insights_get_consent_state(&config, "multipass"),
                  consent ? INSIGHTS_CONSENT_TRUE : INSIGHTS_CONSENT_FALSE);

        MP_LIBINSIGHTS.insights_free(error);
    }
}

TEST_F(LibInsights, consentIsSpecificToSource)
{
    char* error = MP_LIBINSIGHTS.insights_set_consent_state(&config, "multipass", true);

    EXPECT_EQ(error, nullptr) << error;
    EXPECT_EQ(MP_LIBINSIGHTS.insights_get_consent_state(&config, "other"),
              INSIGHTS_CONSENT_UNKNOWN);
    EXPECT_EQ(MP_LIBINSIGHTS.insights_get_consent_state(&config, "multipass"),
              INSIGHTS_CONSENT_TRUE);

    MP_LIBINSIGHTS.insights_free(error);
}
} // namespace multipass::test
