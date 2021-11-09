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

#ifndef MULTIPASS_MOCK_SETTINGS_H
#define MULTIPASS_MOCK_SETTINGS_H

#include "common.h"
#include "mock_singleton_helpers.h"

#include <multipass/settings/settings.h>

namespace multipass::test
{
class MockSettings : public Settings
{
public:
    using Settings::Settings;

    MOCK_METHOD1(register_handler, void(std::unique_ptr<SettingsHandler>));
    MOCK_CONST_METHOD1(get, QString(const QString&));
    MOCK_METHOD2(set, void(const QString&, const QString&));

    MP_MOCK_SINGLETON_BOILERPLATE(MockSettings, Settings);
};
} // namespace multipass::test

#endif // MULTIPASS_MOCK_SETTINGS_H
