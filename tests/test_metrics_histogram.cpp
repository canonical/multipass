/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include <multipass/metrics/metrics_histogram.h>

#include <gmock/gmock.h>

namespace mp = multipass;

using namespace testing;

TEST(MetricsHistogram, memory_histogram_bins)
{
    mp::MetricsHistogram memory_histogram{{512, 1024, 2048, 4096, 8196}};

    memory_histogram.record(256);
    memory_histogram.record(512);
    memory_histogram.record(1024);
    memory_histogram.record(2048);
    memory_histogram.record(2048);
    memory_histogram.record(4196);
    memory_histogram.record(8196);
    memory_histogram.record(16000);

    EXPECT_THAT(memory_histogram.count(0), Eq(1));
    EXPECT_THAT(memory_histogram.count(1), Eq(1));
    EXPECT_THAT(memory_histogram.count(2), Eq(1));
    EXPECT_THAT(memory_histogram.count(3), Eq(2));
    EXPECT_THAT(memory_histogram.count(4), Eq(1));
    EXPECT_THAT(memory_histogram.count(5), Eq(2));
}

TEST(MetricsHistogram, mounts_histogram_bins)
{
    mp::MetricsHistogram mounts_histogram{{1, 2, 4, 8, 16, 32, 64}};

    mounts_histogram.record(0);
    mounts_histogram.record(0);
    mounts_histogram.record(0);
    mounts_histogram.record(1);
    mounts_histogram.record(2);
    mounts_histogram.record(3);
    mounts_histogram.record(4);
    mounts_histogram.record(8);
    mounts_histogram.record(10);

    EXPECT_THAT(mounts_histogram.count(0), Eq(3));
    EXPECT_THAT(mounts_histogram.count(1), Eq(1));
    EXPECT_THAT(mounts_histogram.count(2), Eq(2));
    EXPECT_THAT(mounts_histogram.count(3), Eq(1));
    EXPECT_THAT(mounts_histogram.count(4), Eq(2));
    EXPECT_THAT(mounts_histogram.count(5), Eq(0));
    EXPECT_THAT(mounts_histogram.count(6), Eq(0));
    EXPECT_THAT(mounts_histogram.count(7), Eq(0));
}
