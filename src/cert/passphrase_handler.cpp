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

#include <multipass/passphrase_handler.h>

#include <QByteArray>

#include <openssl/evp.h>

namespace mp = multipass;

constexpr auto hash_size{64};

mp::PassphraseHandler::PassphraseHandler(const Singleton<PassphraseHandler>::PrivatePass& pass) noexcept
    : Singleton<PassphraseHandler>::Singleton{pass}
{
}

QString mp::PassphraseHandler::generate_hash_for(const QString& passphrase) const
{
    QByteArray hash(hash_size, '\0');

    if (!EVP_PBE_scrypt(passphrase.toStdString().c_str(), passphrase.size(), nullptr, 0, 1 << 14, 8, 1, 0,
                        reinterpret_cast<unsigned char*>(hash.data()), hash_size))
        throw std::runtime_error("Cannot generate passphrase hash");

    return QString(hash.toHex());
}
