# About security
> See also: [Authentication](/explanation/authentication), [How to authenticate clients with the Multipass service](/how-to-guides/customise-multipass/authenticate-clients-with-the-multipass-service), [`authenticate`](/reference/command-line-interface/authenticate), [`local.passphrase`](/reference/settings/local-passphrase)

```{caution}
**WARNING**

Multipass is primarily intended for development, testing, and local environments. It is not intended for production use. Review the security considerations in this page carefully before deploying your Multipass VMs.
```

Multipass runs a daemon that is accessed locally via a Unix socket on Linux and macOS, and over a TLS socket on Windows. Anyone with access to the socket can fully control Multipass, which includes mounting host file systems or to tweaking the security features for all instances.

Therefore, make sure to restrict access to the daemon to trusted users.

## Local access to the Multipass daemon

The Multipass daemon runs as root and provides a Unix socket for local communication. Access control for Multipass is initially based on group membership and later by the client's TLS certificate when accepted by providing a set passphrase.

The first client to connect that is a member of the `sudo` group (or `wheel`/`adm`, depending on the OS) will automatically have its TLS certificate imported into the Multipass daemon and will be authenticated to connect.  After this, any other client connecting will need to [`authenticate`](/reference/command-line-interface/authenticate) first by providing a [passphrase](/reference/settings/local-passphrase) set by the administrator.

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/about-security" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

