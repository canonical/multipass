#!/bin/bash
set -e

# Check CPU supports Hypervisor.framework's requirements
# Ref: https://developer.apple.com/documentation/hypervisor
CPU_OK=`sysctl -n kern.hv_support`

if [ "$CPU_OK" -ne "1" ] ; then
    echo "Your CPU is missing required features for the Hypervisor.framework"
    exit 1
fi

if [ $( sw_vers -productVersion | cut -d. -f2 ) -lt 12 ]; then
    echo "Multipass currently requires macOS 10.12 or newer"
    exit 1
fi

exit 0
