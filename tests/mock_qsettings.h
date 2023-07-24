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

#ifndef MULTIPASS_MOCK_QSETTINGS_H
#define MULTIPASS_MOCK_QSETTINGS_H

#include "common.h"
#include "mock_singleton_helpers.h"

#include <src/settings/wrapped_qsettings.h>

namespace multipass::test
{
class MockQSettings : public WrappedQSettings
{
public:
    using WrappedQSettings::WrappedQSettings; // promote visibility
    MOCK_METHOD(QSettings::Status, status, (), (const, override));
    MOCK_METHOD(QString, fileName, (), (const, override));
    MOCK_METHOD(QVariant, value_impl, (const QString& key, const QVariant& default_value),
                (const, override)); // promote visibility
    MOCK_METHOD(void, sync, (), (override));
    MOCK_METHOD(void, setValue, (const QString& key, const QVariant& value), (override));
    MOCK_METHOD(void, remove, (const QString&), (override));
};

class MockQSettingsProvider : public WrappedQSettingsFactory
{
public:
    using WrappedQSettingsFactory::WrappedQSettingsFactory;
    MOCK_METHOD(std::unique_ptr<WrappedQSettings>, make_wrapped_qsettings, (const QString&, QSettings::Format),
                (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockQSettingsProvider, WrappedQSettingsFactory);
};
} // namespace multipass::test
#endif // MULTIPASS_MOCK_QSETTINGS_H
