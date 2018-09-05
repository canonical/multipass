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

#ifndef MULTIPASS_METRICS_HISTOGRAM_H
#define MULTIPASS_METRICS_HISTOGRAM_H

#include <string>
#include <vector>

namespace multipass
{
struct HistogramBins
{
    const std::vector<int> bins;
    const std::vector<std::string> bin_strings;
};

class MetricsHistogram
{
public:
    MetricsHistogram(const std::vector<int>& bin_ranges);
    MetricsHistogram(const MetricsHistogram&) = delete;
    MetricsHistogram& operator=(const MetricsHistogram&) = delete;

    void record(int datum);
    int count(int bin) const;

private:
    const std::vector<int> bin_ranges;
    std::vector<int> bins;
};
} // namespace multipass

#endif // MULTIPASS_METRICS_HISTOGRAM_H
