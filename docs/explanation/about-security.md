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

Multipass runs a daemon that is accessed locally via a Unix socket on Linux and macOS, and over a TLS socket on Windows. Anyone with access to the socket can fully control Multipass, which includes mounting host file systems or to tweaking the security features for all instances.

Therefore, make sure to restrict access to the daemon to trusted users.

## Local access to the Multipass daemon

The Multipass daemon runs as root and provides a Unix socket for local communication. Access control for Multipass is initially based on group membership and later by the user's TLS certificate when accepted by providing a set passphrase.

The first user to connect that is a member of the `sudo` group (or `wheel`/`adm`, depending on the OS) will automatically have their TLS certificate imported into the Multipass daemon and will be authenticated to connect. After this, any other user connecting will need to [`authenticate`](/reference/command-line-interface/authenticate) first by providing a [passphrase](/reference/settings/local-passphrase) set by the administrator.
