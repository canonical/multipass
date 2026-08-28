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

# The CXX bindings are present in every CXX crate, but linking to the CXX
# crate just for the bindings creates an impression of a circular
# dependency that is not correct. The empty crate grants access to the
# bindings while being explicit about the intent.

# When a module in C++ needs to expose types or bindings so that Rust can
# use certain symbols, this crate's CMake library should be added as a
# dependency of that module in CMake. This is only necessary if said
# module does not already depend on another Rust module, or needs to
# include anything but cxx.h symbols
