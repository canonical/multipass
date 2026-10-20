#!/usr/bin/env bash

set -euo pipefail

instance_name=$(/snap/bin/lxc --project snapcraft --format=csv --columns=n list | grep multipass)
echo "instance_name=$instance_name" >> "$GITHUB_ENV"

record_test_result() {
    local status=$1
    local recorded_status=$status
    trap - EXIT
    set +e

    # A later successful test step must not hide an earlier crash from the upload steps.
    if [[ $status == 0 && ${MULTIPASS_TESTS_EXIT_CODE:-0} != 0 ]]; then
        recorded_status=$MULTIPASS_TESTS_EXIT_CODE
    fi
    echo "MULTIPASS_TESTS_EXIT_CODE=$recorded_status" >> "$GITHUB_ENV"
    if /snap/bin/lxc --project snapcraft exec "$instance_name" -- \
        bash -c 'ls -A /coredump 2>/dev/null | grep -q .'; then
        echo "CORE_DUMPS_FOUND=true" >> "$GITHUB_ENV"
    fi
    exit "$status"
}

trap 'record_test_result "$?"' EXIT

prepare_test_coredumps() {
    # GITHUB_ENV carries this flag between test steps; keep earlier steps' dumps.
    if [[ ${MULTIPASS_COREDUMPS_PREPARED:-false} != true ]]; then
        /snap/bin/lxc --project snapcraft exec "$instance_name" -- \
            bash -c 'mkdir -p /coredump && rm -f /coredump/*'
        export MULTIPASS_COREDUMPS_PREPARED=true
        echo "MULTIPASS_COREDUMPS_PREPARED=true" >> "$GITHUB_ENV"
    fi
}
