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

#include <QByteArray>
#include <QFile>
#include <QTemporaryDir>

namespace
{
class UbuntuInsightsAPI : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(directory.isValid());

        consent_dir = directory.filePath("consent").toUtf8();
        insights_dir = directory.filePath("insights").toUtf8();
        system_config_dir = directory.filePath("system").toUtf8();

        config = {consent_dir.constData(),
                  insights_dir.constData(),
                  system_config_dir.constData(),
                  false};
    }

    QTemporaryDir directory;
    QByteArray consent_dir;
    QByteArray insights_dir;
    QByteArray system_config_dir;
    insights_config config{};
};
} // namespace

TEST_F(UbuntuInsightsAPI, missingConsentIsUnknown)
{
    EXPECT_EQ(MP_LIBINSIGHTS.insights_get_consent_state(&config, "multipass"),
              INSIGHTS_CONSENT_UNKNOWN);
}

TEST_F(UbuntuInsightsAPI, consentCanBeCreatedAndUpdated)
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

TEST_F(UbuntuInsightsAPI, consentIsSpecificToSource)
{
    char* error = MP_LIBINSIGHTS.insights_set_consent_state(&config, "multipass", true);

    EXPECT_EQ(error, nullptr) << error;
    EXPECT_EQ(MP_LIBINSIGHTS.insights_get_consent_state(&config, "other"),
              INSIGHTS_CONSENT_UNKNOWN);
    EXPECT_EQ(MP_LIBINSIGHTS.insights_get_consent_state(&config, "multipass"),
              INSIGHTS_CONSENT_TRUE);

    MP_LIBINSIGHTS.insights_free(error);
}
