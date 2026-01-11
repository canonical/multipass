(how-to-guides-troubleshoot-common-issues)=
# Comprehensive Troubleshooting Guide for Common Multipass Issues

This guide summarizes the most frequent issues encountered by Multipass users and provides quick solutions and references to detailed guides.

## 1. Instance Launch/Start Failures
- **Symptoms:** Timeouts, "unknown state" errors, failed image downloads.
- **Quick Fixes:**
  - Check [Troubleshoot launch/start issues](troubleshoot-launch-start-issues)
  - Inspect logs ([Access logs](access-logs))
  - Verify network connectivity

## 2. Networking Problems
- **Symptoms:** Instances unreachable, no IP assigned, SSH failures.
- **Quick Fixes:**
  - See [Troubleshoot networking](troubleshoot-networking)
  - Check DHCP traffic and bridge configuration

## 3. Mounting Issues
- **Symptoms:** Host directories not mounting, errors with encrypted folders.
- **Quick Fixes:**
  - See [Mount an encrypted home folder](mount-an-encrypted-home-folder)
  - Ensure absolute paths are used in GUI

## 4. GUI Problems
- **Symptoms:** GUI not launching, settings not reflected, close behavior issues.
- **Quick Fixes:**
  - Restart Multipass GUI
  - Check for updates and permissions
  - Refer to open issues for known bugs

## 5. Authentication and Permissions
- **Symptoms:** Uninstall script fails, permission denied errors.
- **Quick Fixes:**
  - Run commands with appropriate privileges (e.g., `sudo`)
  - Check user group memberships

## 6. Snap/Packaging Issues
- **Symptoms:** Problems with snap installation, content interface, rpaths.
- **Quick Fixes:**
  - Reinstall Multipass snap
  - Check [Snapcraft documentation](https://snapcraft.io/docs)


## Next Steps

- Review and expand this guide with more specific issues or solutions as needed.
- Submit a pull request to share your contribution with the Multipass community.
- If you need more help, consult the guides linked above or visit the [Multipass documentation](../index.md).
- If your issue persists, search or open an issue on [GitHub](https://github.com/canonical/multipass/issues).

<!-- added by Kiptoo-Deus -->
