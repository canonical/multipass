/*
 * Copyright (C) 2019-2021 Canonical, Ltd.
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

#include "common.h"
#include "mock_settings.h"

#include <src/utils/qsettings_wrapper.h>

#include <multipass/constants.h>
#include <multipass/settings.h>

#include <QString>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

namespace
{
class MockQSettingsWrapper : public mp::QSettingsWrapper
{
public:
    using QSettingsWrapper::QSettingsWrapper; // promote visibility
    MOCK_CONST_METHOD0(status, QSettings::Status());
    MOCK_CONST_METHOD0(fileName, QString());
    MOCK_CONST_METHOD2(value_impl, QVariant(const QString& key, const QVariant& default_value)); // promote visibility
    MOCK_METHOD1(setIniCodec, void(const char* codec_name));
    MOCK_METHOD0(sync, void());
    MOCK_METHOD2(setValue, void(const QString& key, const QVariant& value));
};

class MockQSettingsProvider : public mp::QSettingsProvider
{
public:
    using QSettingsProvider::QSettingsProvider;
    MOCK_CONST_METHOD2(make_qsettings_wrapper,
                       std::unique_ptr<mp::QSettingsWrapper>(const QString&, QSettings::Format));

    MP_MOCK_SINGLETON_BOILERPLATE(MockQSettingsProvider, QSettingsProvider);
};

struct SettingsTest : public Test
{
    MockQSettingsProvider::GuardedMock mock_qsettings_injection = MockQSettingsProvider::inject();
    MockQSettingsProvider* mock_qsettings_provider = mock_qsettings_injection.first;
};

TEST(Settings, provides_get_default_as_get_by_default)
{
    const auto& key = mp::driver_key;
    ASSERT_EQ(MP_SETTINGS.get(key), mpt::MockSettings::mock_instance().get_default(key));
}

TEST(Settings, can_have_get_mocked)
{
    const auto test = QStringLiteral("abc"), proof = QStringLiteral("xyz");
    auto& mock = mpt::MockSettings::mock_instance();

    EXPECT_CALL(mock, get(test)).WillOnce(Return(proof));
    ASSERT_EQ(MP_SETTINGS.get(test), proof);
}

} // namespace
