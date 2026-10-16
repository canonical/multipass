#!/bin/sh
# Written by Multipass
# Check me with shellcheck please!

set -- /run/multipass/mounts/*
[ -e "$1" ] || exit 0

printf '\nHost directories mounted by Multipass:\n\n'
for record in "$@"; do
    source_dir=$(cat "${record}")

    # decode mount target from percent-encoded filename
    target_dir=$(printf '%s' "${record##*/}" | sed 's|%2F|/|g; s|%25|%|g')

    printf ' * %s (on %s)\n' "${source_dir}" "${target_dir}"
done
