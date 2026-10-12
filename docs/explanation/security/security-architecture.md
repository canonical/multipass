(explanation-security-security-architecture)=
# Security architecture

## Multipass architecture and trust boundaries

Multipass has two parts: clients (the CLI and the GUI) that run as your user, and a privileged daemon that runs on the host and manages instances through a hypervisor. See [Reference architecture](explanation-reference-architecture) for a description of each component.

A trust boundary is an interface where data or commands pass between parts of the system with different privileges or owners. Multipass has four trust boundaries:

1. **User and daemon.** Clients run with your user's privileges. The privileged daemon runs as `root` on Linux and macOS, and as `SYSTEM` on Windows. Every request crosses from unprivileged to privileged here. The communication between client and daemon is over gRPC secured using TLS.
2. **Host and internet.** The daemon downloads images from remote image servers using `https` only. Nothing that arrives from the internet is trusted until it is verified using checksums.
3. **Host and instances.** Each instance is a virtual machine, isolated from the host by the hypervisor. The daemon controls instances over SSH. Mounts deliberately cross this boundary to share parts of the host file system with an instance.
4. **Instances and network.** Every instance has a default NAT network that it uses to reach the outside world. Bridged networking adds an interface on a host network, which puts the instance directly on your local network.

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

Mounts are the main way an instance can compromise the host, because the daemon performs them with its own privileges. See [Security considerations](security-considerations-mount) on the Mount page for the implications on each platform.

### Secure development

Multipass is open source, so its security does not depend on keeping the design secret. Changes are proposed and reviewed through pull requests on GitHub, and the code is scanned by static analysis every week. On Linux, a daily job checks the Multipass snap against Ubuntu Security Notices and rebuilds the candidate snap when a bundled package has a fix.

(security-lifecycle)=
## Security lifecycle

Only the latest stable release of Multipass is supported. Older releases do not receive security fixes, so running one might leave you exposed to known vulnerabilities. Security fixes ship in regular feature or maintenance releases, depending on what else they include.

How you get updates depends on your platform:

- **Linux:** the snap updates automatically. To update straight away, run `sudo snap refresh multipass`. You can postpone updates with `sudo snap refresh --hold=<duration> multipass`, but every day you postpone is a day you run without the latest security fixes.
- **macOS and Windows:** Multipass checks for new releases every day and reminds you when one is available. Download and run the new installer to update.

To check that an update has been applied, run `multipass version`.

See [Upgrade](how-to-guides-install-multipass-upgrade) for the full instructions and the [security policy on GitHub](https://github.com/canonical/multipass/blob/main/SECURITY.md) for how security fixes are released.

## Reporting vulnerabilities

To report a vulnerability, follow the private reporting process in the [security policy on GitHub](https://github.com/canonical/multipass/blob/main/SECURITY.md). Published vulnerabilities are listed on the [Multipass security advisories page on GitHub](https://github.com/canonical/multipass/security/advisories) and mentioned in the [release notes](reference-release-notes).
