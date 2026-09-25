(explanation-about-security)=
# About security

> See also: [Authentication](/explanation/authentication), [Reference architecture](/explanation/reference-architecture), [Mount](/explanation/mount), [Security policy on GitHub](https://github.com/canonical/multipass/blob/main/SECURITY.md)

This page explains how Multipass protects your host and your instances, where that protection ends, and what you are responsible for.

```{caution}
Multipass is intended for development, testing, and local environments. It is not intended for production use.
```

## Multipass architecture and trust boundaries

Multipass has two parts: clients (the CLI and the GUI) that run as your user, and a daemon that runs with full privileges on the host and manages instances through a hypervisor. See [Reference architecture](/explanation/reference-architecture) for a description of each component.

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

On Linux and macOS, the daemon listens on a Unix socket. On Windows, it listens on a local TLS socket. At first, only members of the administrator group (`sudo`, `wheel`, or `admin`, depending on the system) can connect. The first administrator to connect has their TLS certificate accepted automatically. After that, every other user must [authenticate](/reference/command-line-interface/authenticate) with a [passphrase](/reference/settings/local-passphrase) set by an administrator before the daemon accepts their requests. See [Authentication](/explanation/authentication) for the details.

### A compromised instance

Multipass uses defense in depth, so that the failure of one layer does not expose the whole host:

- The hypervisor (QEMU, Hyper-V, Apple Virtualization, or VirtualBox) isolates each instance from the host.
- On Linux, the Multipass snap runs under strict [snap confinement](https://snapcraft.io/docs/snap-confinement), which limits what the daemon can reach on the host, including where mounts can point.
- On Windows, mounts are disabled by default.

Mounts are the main way an instance can affect the host, because the daemon performs them with its own privileges. See {ref}`security-considerations-mount` for the implications on each platform.

### Network attackers

Multipass keeps its attack surface small:

- The daemon accepts only local connections. It is not reachable from the network.
- Instances use NAT by default, so other machines cannot connect to them.
- Images are downloaded over HTTPS and checked against their published SHA-256 checksum before use.

### Secure development

Multipass is open source, so its security does not depend on keeping the design secret. Every change goes through public review on GitHub. The code is scanned by static analysis every week, and automated tools track the bundled dependencies and Ubuntu Security Notices so that fixes can be picked up quickly.
