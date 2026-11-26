#!/usr/bin/env python3
#
# Copyright (C) Canonical, Ltd.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 3.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#

"""Merge the per-shard pytest-split duration tables of a CLI test run.

Every shard starts from the previously stored table and updates only the tests
it ran. An entry that differs from the previous table is therefore a fresh
measurement. The merged table is pruned of tests that are no longer collected,
and the shards' JUnit reports are checked to have covered every collected test.
Prints a Markdown summary of the shard wall times to stdout.
"""

import argparse
import json
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def load_table(path):
    if path and Path(path).is_file():
        return json.loads(Path(path).read_text(encoding="utf-8"))
    return {}


def load_collected(path):
    lines = Path(path).read_text(encoding="utf-8").splitlines()
    return {line.strip() for line in lines if "::" in line}


def read_junit(path):
    suites = ET.parse(path).getroot().iter("testsuite")
    tests, seconds = 0, 0.0
    for suite in suites:
        tests += int(suite.get("tests", 0))
        seconds += float(suite.get("time", 0))
    return tests, seconds


def merge(previous, shard_tables, collected):
    merged = dict(previous)
    for table in shard_tables:
        merged.update({k: v for k, v in table.items() if previous.get(k) != v})
    return {k: v for k, v in merged.items() if k in collected}


def summarize(times):
    mean = sum(times) / len(times)
    skew = max(times) / mean if mean else 1.0
    lines = ["| Shard | Wall time |", "|---|---|"]
    lines += [f"| {i} | {t / 60:.1f} min |" for i, t in enumerate(times, 1)]
    lines.append(f"\nSkew (max/mean): **{skew:.2f}**")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--previous", help="Previously stored table")
    parser.add_argument("--collected", required=True,
                        help="Output of `pytest --collect-only -q`")
    parser.add_argument("--shards", type=int, required=True,
                        help="Number of shards that must have reported")
    parser.add_argument("--junit", nargs="+", default=[],
                        help="JUnit report of each shard")
    parser.add_argument("--durations", nargs="*", default=[],
                        help="Table written by each shard")
    parser.add_argument("--out", required=True, help="Merged table to write")
    args = parser.parse_args()

    collected = load_collected(args.collected)
    if not collected:
        sys.exit("error: no tests found in the collected list")

    reports = [read_junit(p) for p in args.junit]
    if len(reports) != args.shards:
        sys.exit(f"error: expected {args.shards} JUnit reports, "
                 f"got {len(reports)}")

    ran = sum(tests for tests, _ in reports)
    print(summarize([seconds for _, seconds in reports]))
    if ran != len(collected):
        sys.exit(f"error: shards ran {ran} tests but {len(collected)} "
                 "were collected")

    merged = merge(load_table(args.previous),
                   [load_table(p) for p in args.durations], collected)
    Path(args.out).write_text(
        json.dumps(merged, indent=2, sort_keys=True) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
