#!/bin/bash
set -e

# Check CPU supports Hypervisor.framework's requirements
# Ref: https://developer.apple.com/documentation/hypervisor
CPU_OK=`sysctl -n kern.hv_support`

# get architecture and major macOS version
arch="$(uname -m)"
os_major="$(sw_vers -productVersion | cut -d. -f1)"

# set minimum required major version per arch
if [ "$arch" = "arm64" ]; then
  min_required=14
  arch_name="Apple Silicon (ARM64)"
else
  min_required=13
  arch_name="Intel (x86_64)"
fi

# TODO: Create an installer plugin to generate a user-friendly dialog if
# the CPU is missing required features for multipass

if [ "$CPU_OK" -ne "1" ] ; then
    echo "Your CPU is missing required features for the Hypervisor.framework"
    exit 1
fi

if [ "$os_major" -lt "$min_required" ]; then
  echo "Multipass currently requires macOS ${min_required} or newer on ${arch_name}"
  exit 1
fi

# Clear the target directories to avoid any leftovers
# Hardcoding the path to avoid the potential for `rm -rf /`
rm -rf "${3}/Library/Application Support/com.canonical.multipass" "${3}/Applications/Multipass.app"
rm -f /Applications/Multipass.app
GUI_AUTOSTART_PLIST="$HOME/Library/LaunchAgents/com.canonical.multipass.gui.autostart.plist"
[ -L $GUI_AUTOSTART_PLIST ] && rm -f $GUI_AUTOSTART_PLIST

exit 0
