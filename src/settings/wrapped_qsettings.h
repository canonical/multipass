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

#ifndef MULTIPASS_WRAPPED_QSETTINGS_H
#define MULTIPASS_WRAPPED_QSETTINGS_H

#include <multipass/disabled_copy_move.h>
#include <multipass/singleton.h>

#include <QSettings>

#include <cassert>

namespace multipass
{
class WrappedQSettingsFactory;
class WrappedQSettings : private DisabledCopyMove
{
public:
    virtual ~WrappedQSettings() = default;

    virtual QSettings::Status status() const
    {
        assert(qsettings);
        return qsettings->status();
    }

    virtual QString fileName() const
    {
        assert(qsettings);
        return qsettings->fileName();
    }

    virtual void sync()
    {
        assert(qsettings);
        qsettings->sync();
    }

    virtual void setValue(const QString& key, const QVariant& value)
    {
        assert(qsettings);
        qsettings->setValue(key, value);
    }

    virtual void remove(const QString& key)
    {
        assert(qsettings);
        qsettings->remove(key);
    }

    QVariant value(const QString& key, const QVariant& default_value = QVariant()) const
    {
        return value_impl(key, default_value); // indirection avoids default args in virtual method
    }

protected:
    WrappedQSettings() = default; // for mocks

    virtual QVariant value_impl(const QString& key, const QVariant& default_value) const
    {
        assert(qsettings);
        return qsettings->value(key, default_value);
    }

private:
    friend class WrappedQSettingsFactory;
    explicit WrappedQSettings(std::unique_ptr<QSettings>&& qsettings) noexcept : qsettings{std::move(qsettings)}
    {
    }

    std::unique_ptr<QSettings> qsettings;
};

class WrappedQSettingsFactory : public Singleton<WrappedQSettingsFactory>
{
public:
    explicit WrappedQSettingsFactory(const Singleton::PrivatePass& pass) : Singleton{pass}
    {
    }

    virtual std::unique_ptr<WrappedQSettings> make_wrapped_qsettings(const QString& file_path,
                                                                     QSettings::Format format) const
    {
        auto qsettings = std::make_unique<QSettings>(file_path, format);
        return std::unique_ptr<WrappedQSettings>(new WrappedQSettings(std::move(qsettings))); /* std::make_unique can't
                                   call private ctors, so we call it ourselves; but the ctor is noexcept, so no leaks */
    }
};
} // namespace multipass

#endif // MULTIPASS_WRAPPED_QSETTINGS_H
