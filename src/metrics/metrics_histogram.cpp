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

#include <algorithm>

namespace mp = multipass;

// bins correlate to bin_ranges as follows:
// - bins.front() (or bins[0]) holds the count for any values less than the first
//   bin_ranges value
// - bins.back() holds the count for any values greater than or equal to the last
//   bin_ranges vaule
// - All other bins elements match the bin_ranges elements
// Example:
//   bin_ranges is initialized as: {512, 1024, 2048, 4096}
//   bin[0] counts values < 512
//   bin[1] counts values 512 <= x < 1024
//   bin[2] counts values 1024 <= x < 2048
//   bin[3] counts values 2048 <= x < 4096
//   bin[4] counts values >= 4096
mp::MetricsHistogram::MetricsHistogram(const std::vector<int>& bin_ranges)
    : bin_ranges{bin_ranges}, bins(bin_ranges.size() + 1)
{
}

void mp::MetricsHistogram::record(int datum)
{
    auto it = std::upper_bound(bin_ranges.begin(), bin_ranges.end(), datum);

    if (it == bin_ranges.end())
        bins.back()++;
    else
        bins[it - bin_ranges.begin()]++;
}

int mp::MetricsHistogram::count(int bin) const
{
    return bins[bin];
}
