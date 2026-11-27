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

#pragma once

#include <QFuture>
#include <QFutureWatcher>
#include <QObject>
#include <QThread>

#include <cassert>
#include <type_traits>

namespace multipass
{

class QtSharedFutureBase : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    Q_SIGNAL void finished();
};

/**
 * @brief A QFuture wrapper that supports multiple independent continuations.
 *
 * Allows multiple consumers to observe the same asynchronous operation by
 * creating independent futures through fork(). Each fork receives the result
 * or exception of the original future without interfering with other forks.
 *
 * Only single-result futures (including QFuture<void>) are supported.
 * Cancellation is not supported.
 *
 * fork() must be called from the thread that owns this object.
 */
template <typename T>
class QtSharedFuture : public QtSharedFutureBase
{
public:
    Q_DISABLE_COPY(QtSharedFuture);

    QtSharedFuture(QFuture<T> future, QObject* parent)
        : QtSharedFutureBase(parent), future{future}, watcher{new QFutureWatcher<T>(this)}
    {
        QObject::connect(watcher,
                         &QFutureWatcher<T>::finished,
                         this,
                         &QtSharedFutureBase::finished,
                         Qt::QueuedConnection);

        watcher->setFuture(future);
    }

    QtSharedFuture(QtSharedFuture&&) = default;
    QtSharedFuture& operator=(QtSharedFuture&&) = default;

    ~QtSharedFuture() = default;

    QFuture<T> fork()
    {
        assert(QThread::currentThread() == thread());

        auto when_finished = future.isFinished()
                               ? QtFuture::makeReadyVoidFuture()
                               : QtFuture::connect(watcher, &QFutureWatcher<T>::finished);

        return when_finished.then([future = this->future]() mutable {
            if constexpr (std::is_same_v<T, void>)
                return future.waitForFinished();
            else
                return future.result();
        });
    }

private:
    QFuture<T> future;
    QFutureWatcher<T>* watcher;
};

} // namespace multipass
