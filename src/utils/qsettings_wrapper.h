/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#ifndef MULTIPASS_QSETTINGS_WRAPPER_H
#define MULTIPASS_QSETTINGS_WRAPPER_H

#include <multipass/disabled_copy_move.h>
#include <multipass/singleton.h>

#include <QSettings>

namespace multipass
{
class QSettingsProvider;
class QSettingsWrapper : private DisabledCopyMove
{
public:
    virtual ~QSettingsWrapper() = default;

    virtual void setIniCodec(const char* codecName)
    {
        assert(qsettings);
        qsettings->setIniCodec(codecName);
    }

protected:
    QSettingsWrapper() = default;

private:
    friend class QSettingsProvider;
    explicit QSettingsWrapper(std::unique_ptr<QSettings>&& qsettings) noexcept : qsettings{std::move(qsettings)}
    {
    }

    std::unique_ptr<QSettings> qsettings;
};

class QSettingsProvider : public Singleton<QSettingsProvider>
{
public:
    explicit QSettingsProvider(const Singleton::PrivatePass& pass) : Singleton{pass}
    {
    }

    virtual std::unique_ptr<QSettingsWrapper> make_qsettings_wrapper(const QString& file_path, QSettings::Format format)
    {
        auto qsettings = std::make_unique<QSettings>(file_path, format);
        return std::unique_ptr<QSettingsWrapper>(new QSettingsWrapper(std::move(qsettings))); /* std::make_unique can't
                                   call private ctors, so we call it ourselves; but the ctor is noexcept, so no leaks */
    }
};
} // namespace multipass

#endif // MULTIPASS_QSETTINGS_WRAPPER_H
