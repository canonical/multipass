#!/usr/bin/env bash

set -euo pipefail

instance_name=$(/snap/bin/lxc --project snapcraft --format=csv --columns=n list | grep multipass)
echo "instance_name=$instance_name" >> "$GITHUB_ENV"

prepare_test_coredumps() {
    /snap/bin/lxc --project snapcraft exec "$instance_name" -- mkdir -p /coredump
}

record_test_result() {
    local status=$1
    trap - EXIT
    set +e

    echo "MULTIPASS_TESTS_EXIT_CODE=$status" >> "$GITHUB_ENV"
    if [[ -n $(/snap/bin/lxc --project snapcraft exec "$instance_name" -- \
        ls -A /coredump 2>/dev/null) ]]; then
        echo "CORE_DUMPS_FOUND=true" >> "$GITHUB_ENV"
    fi
    exit "$status"
}

trap 'record_test_result "$?"' EXIT
