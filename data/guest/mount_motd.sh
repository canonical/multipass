#!/bin/bash
# Copyright (C) Canonical, Ltd.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Written by Multipass
# Check me with shellcheck please!

LC_ALL=C # for deterministic sorting and character classes
replacement=$'\xef\xbf\xbd' # U+FFFD, shown for control characters

set -- /run/multipass/mounts/*
[[ -e "$1" ]] || exit 0

printf '\nFolders mounted from your host by Multipass:\n\n'
for record in "$@"; do
    IFS= read -r source_dir < "${record}"

    # decode mount target from percent-encoded filename
    target_dir=${record##*/}
    target_dir=${target_dir//\%2F//}
    target_dir=${target_dir//\%25/%}

    # neutralize control characters, which could inject terminal escape sequences
    source_dir=${source_dir//[[:cntrl:]]/${replacement}}
    target_dir=${target_dir//[[:cntrl:]]/${replacement}}

    printf ' * %s <- %s\n' "${target_dir}" "${source_dir}"
done
