#!/bin/bash
# Written by Multipass
# Check me with shellcheck please!

set -- /run/multipass/mounts/*
[[ -e "$1" ]] || exit 0

printf '\nHost directories mounted by Multipass:\n\n'
for record in "$@"; do
    IFS= read -r source_dir < "${record}"

    # decode mount target from percent-encoded filename
    target_dir=${record##*/}
    target_dir=${target_dir//\%2F//}
    target_dir=${target_dir//\%25/%}

    printf ' * %s (on %s)\n' "${source_dir}" "${target_dir}"
done
