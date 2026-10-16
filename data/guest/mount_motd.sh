#!/bin/bash
# Written by Multipass
# Check me with shellcheck please!

LC_ALL=C # for deterministic sorting and character classes
replacement=$'\xef\xbf\xbd' # U+FFFD, shown for control characters

set -- /run/multipass/mounts/*
[[ -e "$1" ]] || exit 0

printf '\nHost directories mounted by Multipass:\n\n'
for record in "$@"; do
    IFS= read -r source_dir < "${record}"

    # decode mount target from percent-encoded filename
    target_dir=${record##*/}
    target_dir=${target_dir//\%2F//}
    target_dir=${target_dir//\%25/%}

    # neutralize control characters, which could inject terminal escape sequences
    source_dir=${source_dir//[[:cntrl:]]/${replacement}}
    target_dir=${target_dir//[[:cntrl:]]/${replacement}}

    printf ' * %s (on %s)\n' "${source_dir}" "${target_dir}"
done
