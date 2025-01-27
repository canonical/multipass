(how-to-guides-troubleshoot-mount-an-encrypted-home-folder)=
# Mount an encrypted home folder

<!-- split from https://discourse.ubuntu.com/t/tutorial/27140 -->
<!-- see also https://github.com/canonical/multipass/issues/3537 -->

> See also: [`mount`](/reference/command-line-interface/mount), [Instance](/explanation/instance)

When you create the {ref}`primary-instance`  using `multipass start` or `multipass shell` without additional arguments, Multipass automatically mounts your home directory into it.  

On Linux, if your local home folder is encrypted using ` fscrypt`, [snap confinement](https://snapcraft.io/docs/snap-confinement) prevents you from accessing its contents from a Multipass mount due the peculiar directory structure (`/home/.ecryptfs/<user>/.Private/`). This also applies to the primary instance, where the home folder is mounted automatically.

A workaround is mounting the entire `/home` folder into the instance, using the command: 

```{code-block} text
multipass mount /home primary
```

By doing so, the home folder's contents will be mounted correctly.

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/mount-encrypted-home-folder" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

---

<small>**Contributors:** @ricab, @gzanchi </small>

