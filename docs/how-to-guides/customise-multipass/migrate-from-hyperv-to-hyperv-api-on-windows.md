(how-to-guides-customise-multipass-migrate-from-hyperv-to-hyperv-api-on-windows)=
# Migrate from Hyper-V to the new Hyper-V API driver on Windows

> See also: [`set`](reference-command-line-interface-set),
> [local.driver](reference-settings-local-driver),
> [Driver](explanation-driver),
> [How to set up the driver](how-to-guides-customise-multipass-set-up-the-driver)

As of Multipass 1.17, the `hyperv` driver is being deprecated in favour of a new driver,
`hyperv_api`. While the deprecated driver is restricted to professional editions of Windows, the new
one is built directly on top of Windows APIs like Host Computing System (HCS).

New installs will start with the new driver by default, but existing installs will retain the
previous driver setting. When using `hyperv` as a driver, Multipass will warn users of the
deprecation and ask them to move to `hyperv_api`. To facilitate that, Multipass 1.17 will migrate
`hyperv` instances to `hyperv_api`.

To migrate from `hyperv` to `hyperv_api` and bring your instances along, simply stop them and set
the driver:

```{code-block} text
multipass stop --all
multipass set local.driver=hyperv_api
```

## Repeated driver switches

The original `hyperv` instances are retained until explicitly deleted.

When switching from `hyperv` to `hyperv_api` again, migrated instances are not overwritten. Existing
`hyperv_api` instances remain untouched and instances whose name is taken on the `hyperv_api` side
are not migrated. If, for any reason, you want to repeat a migration, you can achieve that by
deleting the `hyperv_api` counterpart first.

To permanently delete original `hyperv` instances, you can temporarily move back to `hyperv` and use
the `delete` (and purge) command before switching to `hyperv_api` again:

```{code-block} text
multipass set local.driver=hyperv
multipass delete [-p] <instance> [...]
multipass set local.driver=hyperv_api
```

You can choose a convenient time to do all of this. You can also set the driver to `hyperv` and move
back and forth as many times as you want. Apart from the deprecation warning, old functionality
remains the same until the driver is removed entirely. When that happens, it will no longer be
possible to migrate (unless you downgrade to version 1.17).
