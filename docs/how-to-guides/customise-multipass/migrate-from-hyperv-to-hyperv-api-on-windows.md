(how-to-guides-customise-multipass-migrate-from-hyperv-to-hyperv-api-on-windows)=
# Migrate from Hyper-V to the new Hyper-V API driver on Windows

> See also: [`set`](reference-command-line-interface-set),
> [local.driver](reference-settings-local-driver),
> [Driver](explanation-driver),
> [How to set up the driver](how-to-guides-customise-multipass-set-up-the-driver)

As of Multipass 1.17, the `hyperv` driver is being deprecated in favour of a new driver,
`hyperv_api`, which is built directly on top of lower-level Windows APIs — like Host Compute
System (HCS) and Host Compute Network (HCN) — rather than the high-level PowerShell API
that is restricted to professional editions of Windows. New installs will start with the new driver
by default, including on Windows Home, but existing installs will retain the previous driver
setting. Multipass will warn Hyper-V users of the deprecation and ask them to move to `hyperv_api`.
To facilitate that, Multipass 1.17 will migrate `hyperv` instances to `hyperv_api`.

To migrate from `hyperv` to `hyperv_api` and bring your instances along, simply stop them and set
the driver:

```{code-block} text
multipass stop --all
multipass set local.driver=hyperv_api
```

## Repeated driver switches

The original `hyperv` instances are retained until explicitly deleted. You can achieve that by
temporarily moving back to `hyperv` and using the `delete` command:

```{code-block} text
multipass set local.driver=hyperv
multipass delete [-p] <instance> [...]
multipass set local.driver=hyperv_api
```

When switching to `hyperv_api` again, migrated instances are not overwritten. Existing `hyperv_api`
instances remain untouched and instances whose name is taken on the `hyperv_api` side are not
migrated. If, for any reason, you want to repeat a migration, you can achieve that by deleting
the `hyperv_api` counterpart first.

You can choose a convenient time to do all of this. You can also set the driver to `hyperv` and move
back and forth as many times as you want. Apart from the deprecation warning, old functionality
remains the same until the driver is removed entirely. When that happens, it will no longer be
possible to migrate (unless you downgrade to version 1.17).
