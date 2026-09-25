(explanation-about-security)=
# About security

> See also: [Authentication](explanation-authentication), [Reference architecture](explanation-reference-architecture), [Mount](explanation-mount), [Security policy on GitHub](https://github.com/canonical/multipass/blob/main/SECURITY.md)

This page explains how Multipass protects your host and your instances, where that protection ends, and what you are responsible for.

```{caution}
Multipass is intended for development, testing, and local environments. It is not intended for production use.
```

## Multipass architecture and trust boundaries

Multipass has two parts: clients (the CLI and the GUI) that run as your user, and a daemon that runs with full privileges on the host and manages instances through a hypervisor. See [Reference architecture](explanation-reference-architecture) for a description of each component.

A trust boundary is a point where data or commands pass between parts of the system with different privileges or owners. Multipass has four:

```{figure} /images/multipass-security-trust-boundaries.png
   :alt: Diagram of the Multipass client, daemon, instances, image servers, and local network, with four numbered trust boundaries
```

1. **User and daemon.** Clients run as your user. The daemon runs as `root` on Linux and macOS, and as `SYSTEM` on Windows. Every request crosses from unprivileged to privileged here, so the daemon checks who is asking before it acts.
2. **Host and internet.** The daemon downloads images from remote image servers. Nothing that arrives from the internet is trusted until it is verified.
3. **Host and instances.** Each instance is a virtual machine, isolated from the host by the hypervisor. The daemon controls instances over SSH and cloud-init. Mounts deliberately cross this boundary to share host files with an instance.
4. **Instances and network.** By default, instances sit behind NAT and the local network cannot reach them. Bridged networking removes that separation.

## Secure by design

Multipass is designed around the question of who can reach the daemon and what they can do with it. This section describes the threats Multipass is designed against and how it mitigates each one.

### Other users on the same host

Anyone who can talk to the daemon can fully control Multipass, including mounting host folders into instances and changing security settings for all instances. Multipass follows the principle of least privilege to make sure only trusted users get that access.

On Linux and macOS, the daemon listens on a Unix socket. On Windows, it listens on a local TLS socket. At first, only members of the administrator group (`sudo`, `wheel`, or `admin`, depending on the system) can connect. The first administrator to connect has their TLS certificate accepted automatically. After that, every other user must [authenticate](reference-command-line-interface-authenticate) with a [passphrase](reference-settings-local-passphrase) set by an administrator before the daemon accepts their requests. See [Authentication](explanation-authentication) for the details.

### A compromised instance

Multipass uses defense in depth, so that the failure of one layer does not expose the whole host:

- The hypervisor (QEMU, Hyper-V, Apple Virtualization, or VirtualBox) isolates each instance from the host.
- On Linux, the Multipass snap runs under strict [snap confinement](https://snapcraft.io/docs/snap-confinement), which limits what the daemon can reach on the host, including where mounts can point.
- On Windows, mounts are disabled by default.

Mounts are the main way an instance can affect the host, because the daemon performs them with its own privileges. See [Security considerations](security-considerations-mount) on the Mount page for the implications on each platform.

### Network attackers

Multipass keeps its attack surface small:

- The daemon accepts only local connections. It is not reachable from the network.
- Instances use NAT by default, so other machines cannot connect to them.
- Images are downloaded over HTTPS and checked against their published SHA-256 checksum before use.

### Secure development

Multipass is open source, so its security does not depend on keeping the design secret. Every change goes through public review on GitHub. The code is scanned by static analysis every week, and automated tools track the bundled dependencies and Ubuntu Security Notices so that fixes can be picked up quickly.

## Cryptography in Multipass

Multipass uses cryptography to protect the communication between its components, to authenticate users, and to verify downloaded images. It works out of the box and there is nothing to configure.

### Cryptography used by Multipass

| Where | What it protects | Technology |
| ----- | ---------------- | ---------- |
| Client and daemon | Commands and credentials | TLS through gRPC. Clients and the daemon identify themselves with X.509 certificates using EC P-256 keys, signed with SHA-256. |
| Daemon and instances | Control commands, `multipass shell`, `exec`, and `transfer` | SSH with a 2048-bit RSA key that Multipass generates. Ciphers: ChaCha20-Poly1305, with AES-256-CTR as a fallback. |
| Classic mounts | Files shared with an instance | SFTP over the same SSH connection. |
| Image downloads | Images and image metadata | HTTPS with certificate validation. |
| Image integrity | Downloaded images | SHA-256 checksum, when the image server publishes one. The image is discarded if it does not match. |

[Native mounts](explanation-mount-native) use 9P on QEMU and SMB on Hyper-V. Multipass does not add encryption to these, but their traffic stays on the host machine.

### Cryptography you can use directly

The images Multipass launches include an OpenSSH server. You can add your own SSH keys to an instance, for example with cloud-init, and connect with any key type the server supports, such as Ed25519 or RSA.

### Libraries that provide cryptography

- **OpenSSL:** TLS, certificates, and hashing. See the [OpenSSL documentation](https://docs.openssl.org/).
- **libssh:** SSH and SFTP. See the [libssh documentation](https://www.libssh.org/documentation/).
- **gRPC:** encrypted client and daemon communication. See [gRPC authentication](https://grpc.io/docs/guides/auth/).
- **Qt Network:** HTTPS downloads, using OpenSSL.

Multipass bundles these libraries with [vcpkg](https://vcpkg.io/), which builds them from upstream source. Pinned versions and Multipass-specific patches are in the [Multipass repository on GitHub](https://github.com/canonical/multipass/tree/main/3rd-party).

### Protecting data in transit and at rest

**In transit.** Traffic between clients and the daemon, between the daemon and instances, and between the daemon and image servers is encrypted by default. If you run services inside a bridged instance, they are exposed to your local network, so protect them yourself, for example with TLS or SSH.

**At rest.** Multipass does not encrypt instance disks, cached images, or snapshots. To protect them, use full-disk encryption on the host, such as BitLocker on Windows, FileVault on macOS, or LUKS on Linux. See [Configure where Multipass stores external data](how-to-guides-customise-multipass-configure-where-multipass-stores-external-data) and [Mount an encrypted home folder](how-to-guides-troubleshoot-mount-an-encrypted-home-folder).

## Configuring and operating Multipass securely

Multipass is secure by default. Each default below exists for a reason, and changing it has a cost:

- **Only administrators can connect at first.** The daemon socket is limited to the administrator group until the first client is trusted.
- **Other users need a passphrase.** No [passphrase](reference-settings-local-passphrase) is set by default, so no other user can connect. When you set one, anyone who knows it can fully control Multipass. Choose a strong passphrase and share it only with trusted users. See [How to authenticate users with the Multipass service](how-to-guides-customise-multipass-authenticate-users-with-the-multipass-service).
- **Mounts are disabled on Windows.** Enabling [`local.privileged-mounts`](reference-settings-local-privileged-mounts) lets instances write to host folders with `SYSTEM` privileges.
- **Instances use NAT networking.** [Bridged networking](how-to-guides-manage-instances-set-up-custom-networking) puts an instance directly on your local network, where other machines can reach it.
- **The daemon accepts only local connections.** It cannot be reached from the network.
- **Linux installs update automatically.** Holding updates delays security fixes. See [Security lifecycle](security-lifecycle).

### Risks you should be aware of

Some risks come with what Multipass does and cannot be removed by Multipass itself:

- **Control of the daemon means control of the host.** Only give Multipass access to people you would trust with administrator rights.
- **Mounts give instances write access to host files.** Mount only the folders an instance needs, and unmount them when you are done.
- **Bridged instances are on your network.** Treat a bridged instance like any other machine on your network and keep it updated.

### Hardening benchmarks

Multipass does not follow a formal hardening benchmark, such as CIS or FIPS 140-3.

### Logging and monitoring

Multipass logs to the platform's standard logging system: `systemd-journald` on Linux, `/Library/Logs/Multipass` on macOS, and the Event Viewer on Windows. See [Access logs](how-to-guides-troubleshoot-access-logs) and [Logging levels](reference-logging-levels).

## Decommissioning Multipass

Uninstalling Multipass removes the application, but it only removes your instances, images, certificates, and keys if you ask it to:

- **Linux:** run `sudo snap remove --purge multipass`. Without `--purge`, snapd keeps a snapshot of all Multipass data, including instances and keys, for 31 days.
- **macOS:** run the uninstall script and answer "Yes" when it asks whether to delete your VMs and daemon data.
- **Windows:** answer "Yes" when the uninstaller asks whether to remove all data.

Uninstalling never touches the host folders you mounted into instances. Files that you copied into an instance are deleted with the instance.

See the Uninstall section of [Install Multipass](how-to-guides-install-multipass) for the full instructions.

(security-lifecycle)=
## Security lifecycle

Only the latest stable release of Multipass is supported. Older releases do not receive security fixes, so running one leaves you exposed to known vulnerabilities. Security fixes ship in regular releases, which can be bug-fix, minor, or major releases, depending on what else they include.

How you get updates depends on your platform:

- **Linux:** the snap updates automatically. To update straight away, run `sudo snap refresh multipass`. You can postpone updates with `sudo snap refresh --hold=<duration> multipass`, but every day you postpone is a day you run without the latest security fixes.
- **macOS and Windows:** Multipass checks for new releases every day and reminds you when one is available. Download and run the new installer to update.

To check that an update has been applied, run `multipass version`.

See [Upgrade](how-to-guides-install-multipass-upgrade) for the full instructions and the [security policy on GitHub](https://github.com/canonical/multipass/blob/main/SECURITY.md) for how security fixes are released.

## Reporting vulnerabilities

To report a vulnerability, follow the private reporting process in the [security policy on GitHub](https://github.com/canonical/multipass/blob/main/SECURITY.md). Published vulnerabilities are listed on the [Multipass security advisories page on GitHub](https://github.com/canonical/multipass/security/advisories) and mentioned in the [release notes](reference-release-notes).
