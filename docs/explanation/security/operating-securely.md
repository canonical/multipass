(explanation-security-operating-securely)=
# Operating securely

Multipass is secure by default. Each default below exists for a reason, and changing it has a cost:

- **Only trusted clients can control the daemon.** On Linux and macOS, the socket is initially restricted to administrators. On Windows, the local TCP socket is reachable by all local users, but Multipass operations other than `authenticate` require an accepted client certificate.
- **Other users need a passphrase.** No [passphrase](reference-settings-local-passphrase) is set by default, so no other user can connect. When you set one, anyone who knows it can fully control Multipass. Choose a strong passphrase and share it only with trusted users. See [How to authenticate users with the Multipass service](how-to-guides-customise-multipass-authenticate-users-with-the-multipass-service).
- **Mounts are disabled on Windows.** Enabling [`local.privileged-mounts`](reference-settings-local-privileged-mounts) lets instances write to host folders with `SYSTEM` privileges.
- **The daemon listens only locally by default.** It uses a Unix socket on Linux and macOS, and `localhost` on Windows.
- **Linux installs update automatically.** Holding updates delays security fixes. See [Security lifecycle](security-lifecycle).

## Risks you should be aware of

Some risks come with what Multipass does and cannot be removed by Multipass itself:

- **Control of the daemon means control of the host.** Only give Multipass access to people you would trust with administrator rights.
- **Mounts give instances write access to host files.** Mount only the folders an instance needs, and unmount them when you are done.
- **Bridged instances are on your network.** Treat a bridged instance like any other machine on your network and keep it updated.

## Hardening benchmarks

Multipass does not follow a formal host hardening benchmark, such as CIS.

## Logging and monitoring

Multipass logs to the platform's standard logging system: `systemd-journald` on Linux, `/Library/Logs/Multipass` on macOS, and the Event Viewer on Windows. See [Access logs](how-to-guides-troubleshoot-access-logs) and [Logging levels](reference-logging-levels).

## Decommissioning Multipass

Uninstalling Multipass removes the application. Instances, images, certificates, and keys are removed using best efforts using platform specific solutions and only if you ask it to. Data stored in custom location(s) is likely to remain untouched and needs to be deleted separately.

- **Linux:** run `sudo snap remove --purge multipass`. Without `--purge`, snapd keeps a snapshot of all Multipass data, including instances and keys, for 31 days.
- **macOS:** run the uninstall script and answer "Yes" when it asks whether to delete your VMs and daemon data.
- **Windows:** answer "Yes" when the uninstaller asks whether to remove all data.

Uninstalling never touches the host folders you mounted into instances. Files that you copied into an instance are deleted with the instance.

See the Uninstall section of [Install Multipass](how-to-guides-install-multipass) for the full instructions.
