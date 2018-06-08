/*
 * Copyright (C) 2017 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include <multipass/ssh/openssh_key_provider.h>

#include <QDir>
#include <QFile>

#include <memory>


namespace mp = multipass;
namespace
{

QDir make_dir(const QDir& cache_dir, const QString& name)
{
    if (!cache_dir.mkpath(name))
    {
        QString dir{cache_dir.filePath(name)};
        throw std::runtime_error("unable to create directory \'" + dir.toStdString() + "\'");
    }
    return cache_dir.filePath(name);
}

mp::OpenSSHKeyProvider::KeyUPtr create_priv_key(const QString& priv_key_path)
{
    ssh_key priv_key;
    auto ret = ssh_pki_generate(SSH_KEYTYPE_RSA, 2048, &priv_key);
    if (ret != SSH_OK)
        throw std::runtime_error("unable to generate ssh key");

    mp::OpenSSHKeyProvider::KeyUPtr key{priv_key};
    ret = ssh_pki_export_privkey_file(priv_key, nullptr, nullptr, nullptr, priv_key_path.toStdString().c_str());
    if (ret != SSH_OK)
        throw std::runtime_error("failed to export ssh private key to file");

    QFile::setPermissions(priv_key_path, QFile::ReadOwner);
    return key;
}

mp::OpenSSHKeyProvider::KeyUPtr get_priv_key(const QDir& key_dir, const QDir& fallback_dir)
{
    auto priv_key_path = key_dir.filePath("id_rsa");
    auto fallback_key_path = fallback_dir.filePath("id_rsa");

    if (!QFile::exists(priv_key_path) && QFile::exists(fallback_key_path))
    {
        QFile::copy(fallback_key_path, priv_key_path);
    }

    if (QFile::exists(priv_key_path))
    {
        ssh_key priv_key;
        auto imported =
            ssh_pki_import_privkey_file(priv_key_path.toStdString().c_str(), nullptr, nullptr, nullptr, &priv_key);
        if (imported != SSH_OK)
            return create_priv_key(priv_key_path);
        return mp::OpenSSHKeyProvider::KeyUPtr{priv_key};
    }
    return create_priv_key(priv_key_path);
}
}

void mp::OpenSSHKeyProvider::KeyDeleter::operator()(ssh_key key)
{
    ssh_key_free(key);
}

mp::OpenSSHKeyProvider::OpenSSHKeyProvider(const mp::Path& cache_dir, const mp::Path& fallback_dir)
    : ssh_key_dir{make_dir(cache_dir, "ssh-keys")},
      fallback_ssh_key_dir{QDir(fallback_dir).filePath("ssh-keys")},
      priv_key{get_priv_key(ssh_key_dir, fallback_ssh_key_dir)}
{
}

std::string mp::OpenSSHKeyProvider::private_key_as_base64() const
{
    QFile key_file{ssh_key_dir.filePath("id_rsa")};
    auto opened = key_file.open(QIODevice::ReadOnly);
    if (!opened)
        throw std::runtime_error("Unable to open private key file");

    auto data = key_file.readAll();
    size_t data_size = data.length();
    return {data.constData(), data_size};
}

std::string mp::OpenSSHKeyProvider::public_key_as_base64() const
{
    char* base64{nullptr};
    auto ret = ssh_pki_export_pubkey_base64(priv_key.get(), &base64);
    std::unique_ptr<char, decltype(std::free)*> base64_output{base64, std::free};

    if (ret != SSH_OK)
        throw std::runtime_error("unable to export public key as base64");

    return {base64};
}

ssh_key mp::OpenSSHKeyProvider::private_key() const
{
    return priv_key.get();
}
