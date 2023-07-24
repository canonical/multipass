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

#ifndef MULTIPASS_MOCK_NETWORK_H
#define MULTIPASS_MOCK_NETWORK_H

#include "common.h"
#include "mock_singleton_helpers.h"

#include <multipass/url_downloader.h>

#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace multipass::test
{
class MockQNetworkAccessManager : public QNetworkAccessManager
{
public:
    MOCK_METHOD(QNetworkReply*, createRequest, (Operation, const QNetworkRequest&, QIODevice*), (override));
};

class MockQNetworkReply : public QNetworkReply
{
public:
    MockQNetworkReply()
    {
        open(QIODevice::ReadOnly);
    };

    MOCK_METHOD(qint64, readData, (char*, qint64), (override));

    void abort_operation()
    {
        setError(OperationCanceledError, "Operation canceled");
        emit errorOccurred(OperationCanceledError);

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
    MOCK_METHOD(void, abort, (), (override));
};

class MockNetworkManagerFactory : public NetworkManagerFactory
{
public:
    using NetworkManagerFactory::NetworkManagerFactory;

    MOCK_METHOD(std::unique_ptr<QNetworkAccessManager>, make_network_manager, (const Path&), (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockNetworkManagerFactory, NetworkManagerFactory);
};
} // namespace multipass::test
#endif // MULTIPASS_MOCK_NETWORK_H
