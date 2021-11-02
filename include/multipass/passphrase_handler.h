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

#ifndef MULTIPASS_PASSPHRASE_HANDLER_H
#define MULTIPASS_PASSPHRASE_HANDLER_H

#include "singleton.h"

#include <QString>

#define MP_PASSPHRASE_HANDLER multipass::PassphraseHandler::instance()

namespace multipass
{
class PassphraseHandler : public Singleton<PassphraseHandler>
{
public:
    PassphraseHandler(const Singleton<PassphraseHandler>::PrivatePass&) noexcept;

    virtual QString generate_hash_for(const QString& passphrase) const;
};
} // namespace multipass

#endif // MULTIPASS_PASSPHRASE_HANDLER_H
