(how-to-guides-customise-multipass-migrate-from-hyperkit-to-qemu-on-macos)=
# Migrate from Hyperkit to QEMU on macOS

> See also: [`set`](/explanation/driver), [local.driver](/explanation/driver), [Driver](/explanation/driver), [How to set up the driver](/how-to-guides/customise-multipass/set-up-the-driver)

As of Multipass 1.12, the Hyperkit driver is being deprecated. New installs will start with the QEMU driver set by default, but existing installs will retain the previous driver setting. Multipass will warn Hyperkit users of the deprecation and ask them to move to QEMU. To facilitate that, Multipass 1.12 will migrate Hyperkit instances to QEMU.

To migrate from Hyperkit to QEMU and bring your instances along, simply stop them and set the driver to QEMU:

```
multipass stop --all
multipass set local.driver=qemu
```

If you already had QEMU instances, they are not affected by the migration. Instances whose name is taken on the QEMU side are not migrated.

## Repeated driver switches

The original Hyperkit instances are retained until explicitly deleted. You can achieve that by temporarily moving back to Hyperkit and using the delete command:

```
multipass set local.driver=hyperkit
multipass delete [-p] <instance> [...]
multipass set local.driver=qemu
```

When switching to QEMU again, migrated instances are not overwritten. If, for any reason, you want to repeat a migration, you can achieve that by deleting the QEMU counterpart first.

You can choose a convenient time to do any of this and you can set the driver to Hyperkit and move back and forth as many times as you want. Apart from the deprecation warning, functionality remains the same until the driver is removed entirely. When that happens, it will no longer be possible to migrate Multipass (unless you downgrade to version 1.12).

## Demo

Here is a video demonstration of the migration:

[![Hyperkit Migration in Multipass](https://asciinema.org/a/556203.svg)](https://asciinema.org/a/556203)

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://canonical.com/multipass/docs/hyperkit-migration" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*
