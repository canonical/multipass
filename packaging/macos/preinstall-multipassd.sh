#!/bin/bash
set -e

# Check CPU supports Hypervisor.framework's requirements
# Ref: https://developer.apple.com/documentation/hypervisor
CPU_OK=`sysctl -n kern.hv_support`

# TODO: Create an installer plugin to generate a user-friendly dialog if
# the CPU is missing required features for multipass

if [ "$CPU_OK" -ne "1" ] ; then
    echo "Your CPU is missing required features for the Hypervisor.framework"
    exit 1
fi

if [ $( sw_vers -productVersion | cut -d. -f1 ) -lt 13 ]; then
    echo "Multipass currently requires macOS 13 or newer"
    exit 1
fi

# Clear the target directories to avoid any leftovers
# Hardcoding the path to avoid the potential for `rm -rf /`
rm -rf "${3}/Library/Application Support/com.canonical.multipass" "${3}/Applications/Multipass.app"
rm -f /Applications/Multipass.app
GUI_AUTOSTART_PLIST="$HOME/Library/LaunchAgents/com.canonical.multipass.gui.autostart.plist"
[ -L $GUI_AUTOSTART_PLIST ] && rm -f $GUI_AUTOSTART_PLIST

exit 0
