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

#ifndef MULTIPASS_NETWORK_ACCESS_MANAGER_H
#define MULTIPASS_NETWORK_ACCESS_MANAGER_H

#include <QNetworkAccessManager>
#include <QNetworkRequest>

#include <memory>

namespace multipass
{

class NetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT
public:
    using UPtr = std::unique_ptr<NetworkAccessManager>;

    NetworkAccessManager(QObject* parent = nullptr);

protected:
    QNetworkReply* createRequest(Operation op, const QNetworkRequest& orig_request,
                                 QIODevice* outgoingData = nullptr) override;
};
} // namespace multipass

#endif // MULTIPASS_NETWORK_ACCESS_MANAGER_H
