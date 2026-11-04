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

#include <multipass/qt_shared_future.h>

#include "common.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QPromise>

#include <chrono>
#include <exception>
#include <stdexcept>
#include <type_traits>

using namespace testing;

namespace mp = multipass;
namespace mpt = multipass::test;

static constexpr size_t num_forks = 10;
static constexpr int promised_value = 42;
static constexpr auto promised_exception = "mock_error";

namespace
{
template <typename Predicate>
bool process_events_until(Predicate done)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (!done() && std::chrono::steady_clock::now() < deadline)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

    return done();
}

template <typename T>
void expect_success(QFuture<T>& future)
{
    if constexpr (std::is_void_v<T>)
        EXPECT_NO_THROW(future.waitForFinished());
    else
        EXPECT_EQ(future.result(), promised_value);
}

template <typename T>
void expect_exception(QFuture<T>& fork)
{
    MP_EXPECT_THROW_THAT(fork.waitForFinished(),
                         std::runtime_error,
                         mpt::match_what(StrEq(promised_exception)));
}

bool wait_when_all_finished(const auto& futures)
{
    return process_events_until([&] {
        return std::ranges::all_of(futures, [](const auto& future) { return future.isFinished(); });
    });
}

template <typename T>
struct QtSharedFutureTests : testing::Test
{
    void finish_promise()
    {
        if constexpr (!std::is_void_v<T>)
            qpromise.addResult(promised_value);
        qpromise.finish();
    }

    void fail_promise()
    {
        qpromise.setException(std::make_exception_ptr(std::runtime_error{promised_exception}));
        qpromise.finish();
    }

    QPromise<T> qpromise;
};

using ResultTypes = testing::Types<int, void>;
MP_TYPED_TEST_SUITE(QtSharedFutureTests, ResultTypes);
} // namespace

TYPED_TEST(QtSharedFutureTests, pendingForksReceiveTheSameResult)
{
    this->qpromise.start();
    mp::QtSharedFuture<TypeParam> shared{this->qpromise.future(), nullptr};

    std::array<QFuture<TypeParam>, num_forks> forks;
    for (auto& fork : forks)
        fork = shared.fork();

    QCoreApplication::processEvents();
    for (auto& fork : forks)
        EXPECT_FALSE(fork.isFinished());

    this->finish_promise();
    ASSERT_TRUE(wait_when_all_finished(forks));

    for (auto& fork : forks)
        expect_success(fork);
}

TYPED_TEST(QtSharedFutureTests, pendingForksReceiveTheSameException)
{
    this->qpromise.start();
    mp::QtSharedFuture<TypeParam> shared{this->qpromise.future(), nullptr};

    std::array<QFuture<TypeParam>, num_forks> forks;
    for (auto& fork : forks)
        fork = shared.fork();

    QCoreApplication::processEvents();
    for (auto& fork : forks)
        EXPECT_FALSE(fork.isFinished());

    this->fail_promise();
    ASSERT_TRUE(wait_when_all_finished(forks));

    for (auto& fork : forks)
        expect_exception(fork);
}

TYPED_TEST(QtSharedFutureTests, forksOfAnAlreadyFinishedSourceReceiveTheResult)
{
    this->qpromise.start();
    this->finish_promise();

    mp::QtSharedFuture<TypeParam> shared{this->qpromise.future(), nullptr};

    std::array<QFuture<TypeParam>, num_forks> forks;
    for (auto& fork : forks)
        fork = shared.fork();

    for (auto& fork : forks)
        expect_success(fork);
}

TYPED_TEST(QtSharedFutureTests, forksOfAnAlreadyFailedSourceReceiveTheException)
{
    this->qpromise.start();
    this->fail_promise();

    mp::QtSharedFuture<TypeParam> shared{this->qpromise.future(), nullptr};

    std::array<QFuture<TypeParam>, num_forks> forks;
    for (auto& fork : forks)
        fork = shared.fork();

    for (auto& fork : forks)
        expect_exception(fork);
}

TYPED_TEST(QtSharedFutureTests, forkAfterCompletionDoesNotDisturbEarlierForks)
{
    this->qpromise.start();
    mp::QtSharedFuture<TypeParam> shared{this->qpromise.future(), nullptr};

    std::array<QFuture<TypeParam>, num_forks> forks;

    for (size_t i = 0; i < num_forks / 2; i++)
        forks[i] = shared.fork();

    this->finish_promise();

    for (size_t i = num_forks / 2; i < num_forks; i++)
        forks[i] = shared.fork();

    ASSERT_TRUE(wait_when_all_finished(forks));

    for (auto& fork : forks)
        expect_success(fork);
}

TYPED_TEST(QtSharedFutureTests, forksSupportIndependentContinuations)
{
    this->qpromise.start();
    mp::QtSharedFuture<TypeParam> shared{this->qpromise.future(), nullptr};

    std::array<QFuture<size_t>, num_forks> forks;
    for (size_t i = 0; i < num_forks; i++)
        forks[i] = shared.fork().then(&shared, [i](const QFuture<TypeParam>&) { return i; });

    this->finish_promise();
    ASSERT_TRUE(wait_when_all_finished(forks));

    for (size_t i = 0; i < num_forks; i++)
        EXPECT_EQ(forks[i].result(), i);
}

TYPED_TEST(QtSharedFutureTests, failingContinuationDoesNotAffectOtherForks)
{
    this->qpromise.start();
    mp::QtSharedFuture<TypeParam> shared{this->qpromise.future(), nullptr};

    std::array<QFuture<size_t>, num_forks> forks;
    for (size_t i = 0; i < num_forks; i++)
        forks[i] = shared.fork().then(&shared, [i](const QFuture<TypeParam>&) {
            if (i % 2 == 0)
                throw std::runtime_error("continuation failed");
            return i;
        });

    this->finish_promise();
    ASSERT_TRUE(wait_when_all_finished(forks));

    for (size_t i = 0; i < num_forks; i++)
        if (i % 2 == 0)
            MP_EXPECT_THROW_THAT(forks[i].result(),
                                 std::runtime_error,
                                 mpt::match_what(StrEq("continuation failed")));
        else
            EXPECT_EQ(forks[i].result(), i);
}

TYPED_TEST(QtSharedFutureTests, emitsFinishedOnSuccess)
{
    this->qpromise.start();
    bool finished = false;

    mp::QtSharedFuture<TypeParam> shared{this->qpromise.future(), nullptr};
    QObject::connect(&shared, &mp::QtSharedFutureBase::finished, &shared, [&] { finished = true; });

    QCoreApplication::processEvents();
    EXPECT_FALSE(finished);
    this->finish_promise();

    ASSERT_TRUE(process_events_until([&] { return finished; }));
    QCoreApplication::processEvents();
    EXPECT_TRUE(finished);
}

TYPED_TEST(QtSharedFutureTests, emitsFinishedOnFailure)
{
    this->qpromise.start();
    bool finished = false;

    mp::QtSharedFuture<TypeParam> shared{this->qpromise.future(), nullptr};
    QObject::connect(&shared, &mp::QtSharedFutureBase::finished, &shared, [&] { finished = true; });

    QCoreApplication::processEvents();
    EXPECT_FALSE(finished);
    this->fail_promise();

    ASSERT_TRUE(process_events_until([&] { return finished; }));
    QCoreApplication::processEvents();
    EXPECT_TRUE(finished);
}

TYPED_TEST(QtSharedFutureTests, emitsFinishedForAlreadySuccessfulSource)
{
    this->qpromise.start();
    this->finish_promise();

    bool finished = false;
    mp::QtSharedFuture<TypeParam> shared{this->qpromise.future(), nullptr};
    QObject::connect(&shared, &mp::QtSharedFutureBase::finished, &shared, [&] { finished = true; });

    ASSERT_TRUE(process_events_until([&] { return finished; }));
    QCoreApplication::processEvents();
    EXPECT_TRUE(finished);
}

TYPED_TEST(QtSharedFutureTests, emitsFinishedForAlreadyFailedSource)
{
    this->qpromise.start();
    this->fail_promise();

    bool finished = false;
    mp::QtSharedFuture<TypeParam> shared{this->qpromise.future(), nullptr};
    QObject::connect(&shared, &mp::QtSharedFutureBase::finished, &shared, [&] { finished = true; });

    ASSERT_TRUE(process_events_until([&] { return finished; }));
    QCoreApplication::processEvents();
    EXPECT_TRUE(finished);
}
