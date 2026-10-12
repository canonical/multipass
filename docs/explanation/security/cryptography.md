(explanation-security-cryptography)=
# Cryptography

Multipass uses cryptography to protect the communication between its components, to authenticate users, and to verify downloaded images. It works out of the box and there is nothing to configure by the user.

Cryptography technology in use within Multipass:

| Where | What it protects | Technology |
| ----- | ---------------- | ---------- |
| Client and daemon | Commands, payloads, and credentials | TLS through gRPC. Clients and the daemon identify themselves with X.509 certificates using EC P-256 keys, signed with SHA-256. |
| Daemon and instances | Control commands, `multipass shell`, `exec`, and `transfer` | SSH with a 2048-bit RSA key that Multipass generates. Ciphers: ChaCha20-Poly1305, with AES-256-CTR as a fallback. |
| Classic mounts | Files shared with an instance | SFTP over the same SSH connection. |
| Native mounts | Files shared with an instance | 9P on QEMU and SMB on Hyper-V. Multipass does not add encryption to these, but their traffic stays on the host machine. |
| Image downloads | Images and image metadata | HTTPS with certificate validation. |
| Image integrity | Downloaded images | SHA-256 or SHA-512 checksum, when the image source provides one. The image is discarded if it does not match. |

## Cryptography you can use directly

The images Multipass launches include an OpenSSH server. You can add your own SSH keys to an instance, for example with cloud-init, and connect with any key type the server supports, such as Ed25519 or RSA.

## Libraries that use cryptography

- **OpenSSL:** TLS, certificates, and hashing. See the [OpenSSL documentation](https://docs.openssl.org/).
- **libssh:** SSH and SFTP. See the [libssh documentation](https://www.libssh.org/documentation/).
- **gRPC:** encrypted client and daemon communication. See [gRPC authentication](https://grpc.io/docs/guides/auth/).

Multipass bundles these libraries with [vcpkg](https://vcpkg.io/), which builds them from upstream source. Pinned versions and Multipass-specific patches are in the [Multipass repository on GitHub](https://github.com/canonical/multipass/tree/main/3rd-party).

## Protecting data in transit and at rest

**In transit.** Traffic between clients and the daemon, between the daemon and instances, and between the daemon and image servers is encrypted by default. If you run services inside a bridged instance, they are exposed to your local network, so protect them yourself, for example with TLS or SSH.

**At rest.** Multipass does not encrypt instance disks, cached images, or snapshots. To protect them, use full-disk encryption on the host, such as BitLocker on Windows, FileVault on macOS, or LUKS on Linux. See [Configure where Multipass stores external data](how-to-guides-customise-multipass-configure-where-multipass-stores-external-data) and [Mount an encrypted home folder](how-to-guides-troubleshoot-mount-an-encrypted-home-folder).
