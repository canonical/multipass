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

import math


def is_within_tolerance(actual, expected, lb_tolerance=0.1, ub_tolerance=0.02):
    # Compute log-scaled decay from 25% at 1024 down to 10% at 10240
    # This is to tolerate more at lower values.
    log_scale = math.log(10240 / expected) / math.log(10240 / 1024)
    tol = max(lb_tolerance, lb_tolerance + (0.25 - lb_tolerance) * log_scale)

    lower_bound = int(expected * (1 - tol))
    upper_bound = int(expected * (1 + ub_tolerance))

    return lower_bound <= int(actual) <= upper_bound
