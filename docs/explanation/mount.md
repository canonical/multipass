(explanation-mount)=
# Mount

> See also: [`mount`](/reference/command-line-interface/mount), [How to share data with an instance](/how-to-guides/manage-instances/share-data-with-an-instance)

In Multipass, a **mount** is a directory mapping from the host to an [instance](/explanation/instance), making its contents, and changes therein, available on both ends. Make sure to review the {ref}`security-considerations-mount` below.

In Multipass, there are two types of mounts: classic (default) and native.
* {ref}`explanation-mount-classic` use technology built into Multipass and thus allow for higher compatibility, while slightly reduced performance.
* {ref}`explanation-mount-native`, on the other hand, use hypervisor or platform-specific mounts to offer better performance, but limited compatibility.

(explanation-mount-classic)=
## Classic mounts

Classic mounts use SSHFS (SSH File System) to achieve file/directory sharing. This option is available across all our backends.

SSHFS is based on SSH, which pays a performance penalty to achieve secure communication.

(explanation-mount-native)=
## Native mounts

Native mounts use driver-dependent technologies to achieve the high performance. They are only available in the following cases:

- On **Hyper-V**, where they are implemented with [SMB/CIFS](https://learn.microsoft.com/en-us/windows/win32/fileio/microsoft-smb-protocol-and-cifs-protocol-overview).
- On **QEMU**, where they are implemented with [9P](https://en.wikipedia.org/wiki/9P_(protocol)).
- On **LXD** (deprecated), using that backend's own mounts, which also rely on [9P](https://en.wikipedia.org/wiki/9P_(protocol)).

> See also: {ref}`driver-feature-disparities`.

(security-considerations-mount)=
## Security considerations

`````{tab-set}

````{tab-item} Linux
Because mounts are performed as `root` -- unless installed via snap, see below -- they allow write access to the whole host operating system. But since only privileged users (members of `sudo`, `wheel`, `admin` groups) can use Multipass, this isn't a concern on Linux.

If Multipass is installed via snap package, [snap confinement](https://snapcraft.io/docs/snap-confinement) prevents mounts outside of the `/home` directory (and to hidden files/folders in the `/home` directory) and possibly, removable media (depending on the connected interfaces). Still, a user (A) with access to Multipass could access mounts that a different user (B) established to B's home directory (that is, outside of A's home).
````

````{tab-item} macOS
Because mounts are performed as `root`, they allow write access to the whole host operating system. But since only privileged users (members of `sudo`, `wheel`, `admin` groups) can use Multipass, this isn't a concern on macOS.
````

````{tab-item} Windows
Because mounts are performed as privileged users (`SYSTEM` on Windows), they allow write access to the whole host operating system.

For historical reasons, mounts are disabled by default on Windows, even though in the current version of Multipass users need to authenticate with the daemon before it will service their requests. See [`local.privileged-mounts`](/reference/settings/local-privileged-mounts) for information on how to enable them if needed.

<!-- OLD CONTENT
as anyone with TCP access to `localhost` (`127.0.0.1`) can use Multipass, and by extension, gets access to the entire filesystem.
-->

````

`````

<!-- Discourse contributors
<small>**Contributors:** @tmihoc, @georgeliaojia, @ricab, @sharder996, @davidekete, @gzanchi </small>
-->
