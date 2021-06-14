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

#ifndef MULTIPASS_MOCK_NETWORK_H
#define MULTIPASS_MOCK_NETWORK_H

#include "mock_singleton_helpers.h"

#include <multipass/url_downloader.h>

#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <gmock/gmock.h>

namespace multipass::test
{
class MockQNetworkAccessManager : public QNetworkAccessManager
{
public:
    MOCK_METHOD3(createRequest, QNetworkReply*(Operation, const QNetworkRequest&, QIODevice*));
};

class MockQNetworkReply : public QNetworkReply
{
public:
    MockQNetworkReply()
    {
        open(QIODevice::ReadOnly);
    };

    MOCK_METHOD2(readData, qint64(char*, qint64));

    void abort_operation()
    {
        setError(OperationCanceledError, "Operation canceled");
        emit error(OperationCanceledError);

        setFinished(true);
        emit finished();
    };

    void set_error(QNetworkReply::NetworkError errorCode, const QString& errorString)
    {
        setError(errorCode, errorString);
    };

    void set_attribute(QNetworkRequest::Attribute code, const QVariant& value)
    {
        setAttribute(code, value);
    };

    void set_header(QNetworkRequest::KnownHeaders header, const QVariant& value)
    {
        setHeader(header, value);
    }

public Q_SLOTS:
    MOCK_METHOD0(abort, void());
};

class MockNetworkManagerFactory : public NetworkManagerFactory
{
public:
    using NetworkManagerFactory::NetworkManagerFactory;

    MOCK_CONST_METHOD1(make_network_manager, std::unique_ptr<QNetworkAccessManager>(const Path&));

    MP_MOCK_SINGLETON_BOILERPLATE(MockNetworkManagerFactory, NetworkManagerFactory);
};
} // namespace multipass::test
#endif // MULTIPASS_MOCK_NETWORK_H
