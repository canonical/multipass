#!/bin/sh
set -eu

if [ "$EUID" -ne 0 ]; then
    echo "This script needs to run as root"
    exit 1
fi

while true; do
    read -p "Are you sure you want to remove Multipass from your system? [Y/N] " yn
    case $yn in
        [Yy]* ) break;;
        [Nn]* ) echo "Aborted"; exit;;
        * ) echo "Please answer yes or no.";;
    esac
done

DELETE_VMS=0

while true; do
    read -p "Do you want to delete all your Multipass VMs and daemon data too? [Y/N] " yn
    case $yn in
        [Yy]* ) DELETE_VMS=1; break;;
        [Nn]* ) DELETE_VMS=0; break;;
        * ) echo "Please answer yes or no.";;
    esac
done

if [ $DELETE_VMS -eq 1 ]; then
    echo "Removing VMs:"
    sudo -u "$(logname)" multipass delete -vv --purge --all || echo "Failed to delete multipass VMs from underlying driver" >&2

fi

LAUNCH_AGENT_DEST="/Library/LaunchDaemons/com.canonical.multipassd.plist"

echo .
echo "Removing the Multipass daemon launch agent:"
launchctl unload -w "$LAUNCH_AGENT_DEST"

if [ $DELETE_VMS -eq 1 ]; then
    echo "Removing daemon data:"
    rm -rfv "/var/root/Library/Application Support/multipassd"
    rm -rfv "/var/root/Library/Application Support/multipass-client-certificate"
    rm -rfv "/var/root/Library/Preferences/multipassd"
    rm -fv "/Library/Keychains/multipass_root_cert.pem"
fi

echo .
echo "Removing Multipass:"
rm -fv "$LAUNCH_AGENT_DEST"

rm -fv /usr/local/bin/multipass
rm -rfv /Applications/Multipass.app

rm -rfv "/Library/Application Support/com.canonical.multipass"
rm -rfv "/var/root/Library/Caches/multipassd"

# GUI Autostart
rm -fv "$HOME/Library/LaunchAgents/com.canonical.multipass.gui.autostart.plist"

# User-specific client certificates and GUI data
rm -rfv "$HOME/Library/Application Support/multipass-client-certificate"
rm -rfv "$HOME/Library/Application Support/com.canonical.multipassGui"
rm -rfv "$HOME/Library/Preferences/multipass"

# Shell completions
# Bash completions
rm -fv "/usr/local/etc/bash_completion.d/multipass"
rm -fv "/opt/local/share/bash-completion/completions/multipass"

# Zsh completions
rm -fv "/usr/local/share/zsh/site-functions/_multipass"
rm -fv "/opt/local/share/zsh/site-functions/_multipass"

# Fish completions
rm -fv "/usr/local/share/fish/vendor_completions.d/multipass.fish"
rm -fv "/opt/local/share/fish/vendor_completions.d/multipass.fish"

# Log files
rm -rfv "/Library/Logs/Multipass"

echo .
echo "Removing package installation receipts"
rm -fv "/private/var/db/receipts/com.canonical.multipass.multipassd.bom"
rm -fv "/private/var/db/receipts/com.canonical.multipass.multipassd.plist"
rm -fv "/private/var/db/receipts/com.canonical.multipass.multipass.bom"
rm -fv "/private/var/db/receipts/com.canonical.multipass.multipass.plist"

echo .
echo "Uninstall complete"

if [ $DELETE_VMS -eq 0 ]; then
    echo "Your Multipass VMs were preserved in /var/root/Library/Application Support/multipassd"
fi
