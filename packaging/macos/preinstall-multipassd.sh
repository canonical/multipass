#!/bin/bash
set -e

# Check CPU supports Hypervisor.framework's requirements
# Ref: https://developer.apple.com/documentation/hypervisor
CPU_OK=`sysctl -n kern.hv_support`

if [ "$CPU_OK" -ne "1" ] ; then
    # TODO: Create an installer plugin to generate a user-friendly dialog if
    # the CPU is missing required features for multipass
    echo "Your CPU is missing required features for the Hypervisor.framework"
    exit 1
fi

exit 0
